// SPDX-License-Identifier: Apache-2.0

//===- RewriteONNXForZHigh.td - Rewrite ONNX Ops for ZHigh -*- tablegen ---===//
//
// Copyright 2019-2020 The IBM Research Authors.
//
// =============================================================================
//
// Defines language-specific pattern match rewritings for ONNX using
// Declarative Rewrite Rules (DRR) specified using TableGen records.
//
//===----------------------------------------------------------------------===//

#ifndef REWRITE_ONNX_FOR_ZHIGH
#define REWRITE_ONNX_FOR_ZHIGH

#ifndef OP_BASE
include "src/Dialect/ONNX/ONNX.td"
include "src/Accelerators/NNPA/Dialect/ZHigh/ZHigh.td"
include "src/Accelerators/NNPA/Conversion/ONNXToZHigh/ONNXToZHighCommon.td"
#endif // OP_BASE

/// Note: The DRR definition used for defining patterns is shown below:
///
/// class Pattern<
///    dag sourcePattern, list<dag> resultPatterns,
///    list<dag> additionalConstraints = [],
///    dag benefitsAdded = (addBenefit 0)
/// >;

// Check the rank of a value is greater than a given integer.
class HasRankGT<int rank> :
  Constraint<CPred<"isRankedShapedType($0.getType()) && "
                   "(getRank($0.getType()) > " # rank # ")">>;

// Check the rank of a value is of a given integer.
class HasRankOf<int rank> :
  Constraint<CPred<"isRankedShapedType($0.getType()) && "
                   "(getRank($0.getType()) == " # rank # ")">>;

def GetSqrtResultBatchNormA :
      NativeCodeCall<"getSqrtResultBatchNormA($_loc, $_builder, $0, $1)">;

//===----------------------------------------------------------------------===//
// Rewrite
//
// ONNXBatchNormalizationInferenceModeOp %X
//             = NHWCtoNCHW
//                 (ZHighUnstickOp
//                     (ZHighBatchNormOp
//                         (ZHighStickOp
//                             (NCHWtoNHWC %X)),
//                         (ZHighStickOp %A),
//                         (ZHighStickOp %B)))
//
// %A = $scale / sqrt($var + $epsilon))
// %B = $b - $mean * %A
//===----------------------------------------------------------------------===//
def replaceONNXBatchNormalizationInferenceModePattern : Pattern<
  (ONNXBatchNormalizationInferenceModeOp:$res $x, $scale, $b, $mean, $var,
                                         $epsilon, $momentum),
  [
    // $A = scale / sqrt(var + epsilon)
    (ONNXDivOp:$A $scale, (GetSqrtResultBatchNormA $var, $epsilon)),
    // $B = bias - mean * $A
    (ONNXSubOp:$B $b, (ONNXMulOp $mean, $A)),

    // Calculate BatchNorm Op using $A and $B
    (NHWCtoNCHW (GetTypeOf $res),
        (ZHighUnstickOp
            (ZHighBatchNormOp
                (ZHighStickOp (NCHWtoNHWC $x), (NHWCLayoutAttr)),
                (ZHighStickOp $A, (_1DLayoutAttr)),
                (ZHighStickOp $B, (_1DLayoutAttr)))))
  ]
>;

//===----------------------------------------------------------------------===//
// These rules are to eliminate broadcasting that is not supported by NNPA.
//
// Rewrite `BinaryOp(lhs, rhs)` if one of the two inputs is a constant and
// unidirectional broadcastable to the other input.
// For example: lhs is a constant of shape [8] and rhs is of shape [1x4x8].
// 
// Taking ONNXAddOp as an example, we rewrite it as follows:
//
// 1. `ONNXAddOp %constant, %X` will be canonicalized to `ONNXAddOp %X, %constant`
// 2. `ONNXAddOp %X, %constant` will be replaced by
//       `ONNXAddOp %X, (ONNXExpandOp %constant, (ONNXShapeOp %X))`
//
// 
//===----------------------------------------------------------------------===//

// Get a type for a tensor that stores the shape of another tensor.
def GetShapeTypeOf: NativeCodeCall<
  "RankedTensorType::get({$0.getType().cast<ShapedType>().getRank()}, $_builder.getIntegerType(64))"
>;

// Check unidirectional broadcasting from the first to second tensor.
def IsUniBroadcastingFromFirstToSecond: Constraint<
  CPred<"isUniBroadcatableFirstToSecond($0, $1)">,
  "Is unidirectional broadcasting from the first to second tensor"
>;

//===----------------------------------------------------------------------===//
// For ONNXAddOp
//===----------------------------------------------------------------------===//

def expandConstantOperandForAddOp1: Pat<
  (ONNXAddOp (ONNXConstantOp:$c $_, $_, $_, $_, $_, $_, $_, $_), $x),
  (ONNXAddOp $x, $c),
  [(IsUniBroadcastingFromFirstToSecond $c, $x)]
>;

def expandConstantOperandForAddOp2: Pat<
  (ONNXAddOp $x, (ONNXConstantOp:$c $_, $_, $_, $_, $_, $_, $_, $_)),
  (ONNXAddOp $x, (ONNXExpandOp $c,
                               (ONNXShapeOp $x, (returnType (GetShapeTypeOf $x))),
                               (returnType $x))),
  [(IsUniBroadcastingFromFirstToSecond $c, $x)]
>;

//===----------------------------------------------------------------------===//
// For ONNXDivOp
//===----------------------------------------------------------------------===//

def expandConstantOperandForDivOp1: Pat<
  (ONNXDivOp $x, (ONNXConstantOp:$c $_, $_, $_, $_, $_, $_, $_, $_)),
  (ONNXDivOp $x, (ONNXExpandOp $c,
                               (ONNXShapeOp $x, (returnType (GetShapeTypeOf $x))),
                               (returnType $x))),
  [(IsUniBroadcastingFromFirstToSecond $c, $x)]
>;

def expandConstantOperandForDivOp2: Pat<
  (ONNXDivOp (ONNXConstantOp:$c $_, $_, $_, $_, $_, $_, $_, $_), $x),
  (ONNXDivOp (ONNXExpandOp $c,
                           (ONNXShapeOp $x, (returnType (GetShapeTypeOf $x))),
                           (returnType $x)),
             $x),
  [(IsUniBroadcastingFromFirstToSecond $c, $x)]
>;

//===----------------------------------------------------------------------===//
// For ONNXMulOp
//===----------------------------------------------------------------------===//

def expandConstantOperandForMulOp1: Pat<
  (ONNXMulOp (ONNXConstantOp:$c $_, $_, $_, $_, $_, $_, $_, $_), $x),
  (ONNXMulOp $x, $c),
  [(IsUniBroadcastingFromFirstToSecond $c, $x)]
>;

def expandConstantOperandForMulOp2: Pat<
  (ONNXMulOp $x, (ONNXConstantOp:$c $_, $_, $_, $_, $_, $_, $_, $_)),
  (ONNXMulOp $x, (ONNXExpandOp $c,
                               (ONNXShapeOp $x, (returnType (GetShapeTypeOf $x))),
                               (returnType $x))),
  [(IsUniBroadcastingFromFirstToSecond $c, $x)]
>;

//===----------------------------------------------------------------------===//
// For ONNXSubOp
//===----------------------------------------------------------------------===//

def expandConstantOperandForSubOp1: Pat<
  (ONNXSubOp $x, (ONNXConstantOp:$c $_, $_, $_, $_, $_, $_, $_, $_)),
  (ONNXSubOp $x, (ONNXExpandOp $c,
                               (ONNXShapeOp $x, (returnType (GetShapeTypeOf $x))),
                               (returnType $x))),
  [(IsUniBroadcastingFromFirstToSecond $c, $x)]
>;

def expandConstantOperandForSubOp2: Pat<
  (ONNXSubOp (ONNXConstantOp:$c $_, $_, $_, $_, $_, $_, $_, $_), $x),
  (ONNXSubOp (ONNXExpandOp $c,
                           (ONNXShapeOp $x, (returnType (GetShapeTypeOf $x))),
                           (returnType $x)),
             $x),
  [(IsUniBroadcastingFromFirstToSecond $c, $x)]
>;

//===----------------------------------------------------------------------===//
// Rules to turn ONNXMatMulOp with N-D inputs into the one with 3-D inputs.
//===----------------------------------------------------------------------===//

def ReshapeTo3D: NativeCodeCall<
  "reshapeTo3D($_builder, $_loc, $0)"
>;

def GetMatMulResultShape: NativeCodeCall<
  "getMatMulResultShape($_builder, $_loc, $0, $1)"
>;

def GetMatMulResultType: NativeCodeCall<
  "getMatMulResultType($_builder, $_loc, $0, $1)"
>;

def rewriteMatMulNDto3D_NonBroadcast: Pat<
  (ONNXMatMulOp:$c $a, $b),
  (ONNXReshapeOp (ONNXMatMulOp
                    (ReshapeTo3D:$a_r $a),
                    (ReshapeTo3D:$b_r $b),
                    (returnType (GetMatMulResultType $a_r, $b_r))),
                 (GetMatMulResultShape $a, $b),
                 (GetZeroI64Attr)),
  [(HasRankGT<3> $a), (HasRankGT<3> $b)]
>;

def rewriteMatMulNDto3D_Broadcast_1: Pat<
  (ONNXMatMulOp:$c $a, $b),
  (ONNXReshapeOp (ONNXMatMulOp
                    (ReshapeTo3D:$a_r $a),
                    $b,
                    (returnType (GetMatMulResultType $a_r, $b))),
                 (GetMatMulResultShape $a, $b),
                 (GetZeroI64Attr)),
  [(HasRankGT<3> $a), (HasRankOf<2> $b)]
>;

def rewriteMatMulNDto3D_Broadcast_2: Pat<
  (ONNXMatMulOp:$c $a, $b),
  (ONNXReshapeOp (ONNXMatMulOp
                    $a,
                    (ReshapeTo3D:$b_r $b),
                    (returnType (GetMatMulResultType $a, $b_r))),
                 (GetMatMulResultShape $a, $b),
                 (GetZeroI64Attr)),
  [(HasRankOf<2> $a), (HasRankGT<3> $b)]
>;

#endif // REWRITE_ONNX_FOR_ZHIGH
