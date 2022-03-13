// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "qorder_unary_ops.h"
#include "qorder_unary_ops_impl.h"

namespace onnxruntime {
namespace contrib {
namespace cuda {

using namespace onnxruntime::cuda;

ONNX_OPERATOR_KERNEL_EX(
    QOrderedGelu,
    kMSDomain,
    1,
    kCudaExecutionProvider,
    (*KernelDefBuilder::Create())
        .TypeConstraint("Q", DataTypeImpl::GetTensorType<int8_t>())
        .TypeConstraint("S", DataTypeImpl::GetTensorType<float>())
        .InputMemoryType(OrtMemTypeCPUInput, 1)   // scale_input
        .InputMemoryType(OrtMemTypeCPUInput, 2),  // scale_output
    QOrderedGelu);

Status
QOrderedGelu::ComputeInternal(OpKernelContext* context) const {
  CUDA_RETURN_IF_ERROR(cudaDeviceSynchronize());

  const Tensor& input = *context->Input<Tensor>(0);
  const float* scale_input = context->Input<Tensor>(1)->Data<float>();
  const float* scale_output = context->Input<Tensor>(2)->Data<float>();
  const auto& shape = input.Shape();
  Tensor* output = context->Output(0, shape);
  cudaStream_t stream = Stream();
  QOrderUnary_Gelu(stream, input.Data<int8_t>(), scale_input, output->MutableData<int8_t>(), scale_output, (size_t)(shape.Size()));

  CUDA_RETURN_IF_ERROR(cudaDeviceSynchronize());
  return Status::OK();
}

}  // namespace cuda
}  // namespace contrib
}  // namespace onnxruntime