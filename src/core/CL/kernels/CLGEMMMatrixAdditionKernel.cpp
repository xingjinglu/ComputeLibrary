/*
 * Copyright (c) 2017-2018 ARM Limited.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "arm_compute/core/CL/kernels/CLGEMMMatrixAdditionKernel.h"

#include "arm_compute/core/CL/CLHelpers.h"
#include "arm_compute/core/CL/CLKernelLibrary.h"
#include "arm_compute/core/CL/ICLTensor.h"
#include "arm_compute/core/CL/OpenCL.h"
#include "arm_compute/core/Error.h"
#include "arm_compute/core/FixedPoint.h"
#include "arm_compute/core/Helpers.h"
#include "arm_compute/core/Types.h"
#include "arm_compute/core/Validate.h"
#include "arm_compute/core/Window.h"

using namespace arm_compute;

namespace
{
std::pair<Status, Window> validate_and_configure_window(ITensorInfo *input, ITensorInfo *output)
{
    const unsigned int num_elems_processed_per_iteration = max_cl_vector_width / data_size_from_type(input->data_type());
    // Configure kernel window
    Window win = calculate_max_window(*input, Steps(num_elems_processed_per_iteration));

    AccessWindowHorizontal input_access(input, 0, num_elems_processed_per_iteration);
    AccessWindowHorizontal output_access(output, 0, num_elems_processed_per_iteration);

    bool window_changed = update_window_and_padding(win, input_access, output_access);

    output_access.set_valid_region(win, input->valid_region());

    Status err = (window_changed) ? ARM_COMPUTE_CREATE_ERROR(ErrorCode::RUNTIME_ERROR, "Insufficient Padding!") : Status{};
    return std::make_pair(err, win);
}
} // namespace

namespace
{
Status validate_arguments(const ITensorInfo *input, const ITensorInfo *output, const float beta)
{
    ARM_COMPUTE_RETURN_ERROR_ON_NULLPTR(input, output);

    ARM_COMPUTE_RETURN_ERROR_ON_DATA_TYPE_CHANNEL_NOT_IN(input, 1, DataType::QS8, DataType::QS16, DataType::F16, DataType::F32);
    ARM_COMPUTE_RETURN_ERROR_ON_MISMATCHING_DATA_TYPES(input, output);
    ARM_COMPUTE_RETURN_ERROR_ON(input->dimension(0) != output->dimension(0));
    ARM_COMPUTE_RETURN_ERROR_ON(input->dimension(1) != output->dimension(1));

    ARM_COMPUTE_UNUSED(beta);
    return Status{};
}
} // namespace

CLGEMMMatrixAdditionKernel::CLGEMMMatrixAdditionKernel()
    : _input(nullptr), _output(nullptr)
{
}

void CLGEMMMatrixAdditionKernel::configure(const ICLTensor *input, ICLTensor *output, float beta)
{
    ARM_COMPUTE_ERROR_ON_NULLPTR(input, output);

    // Perform validation step
    ARM_COMPUTE_ERROR_THROW_ON(validate_arguments(input->info(), output->info(), beta));

    _input  = input;
    _output = output;

    std::ostringstream ma_arguments;
    if(is_data_type_fixed_point(input->info()->data_type()))
    {
        ma_arguments << "-DBETA=" << (input->info()->data_type() == DataType::QS8 ?
                                      sqcvt_qs8_f32(beta, input->info()->fixed_point_position()) :
                                      sqcvt_qs16_f32(beta, input->info()->fixed_point_position()))
                     << " ";
        ma_arguments << "-DFIXED_POINT_POSITION=" << input->info()->fixed_point_position();
    }
    else
    {
        ma_arguments << "-DBETA=" << beta;
    }

    std::set<std::string> build_opts;
    build_opts.emplace(ma_arguments.str());

    // Create kernel
    std::string data_type_name = lower_string(string_from_data_type(input->info()->data_type()));
    _kernel                    = static_cast<cl::Kernel>(CLKernelLibrary::get().create_kernel(("gemm_ma_" + data_type_name), build_opts));

    // Configure kernel window
    auto win_config = validate_and_configure_window(input->info(), output->info());
    ARM_COMPUTE_ERROR_THROW_ON(win_config.first);
    ICLKernel::configure(win_config.second);
}

Status CLGEMMMatrixAdditionKernel::validate(const ITensorInfo *input, const ITensorInfo *output, const float beta)
{
    ARM_COMPUTE_RETURN_ERROR_ON(validate_arguments(input, output, beta));
    return Status{};
}

void CLGEMMMatrixAdditionKernel::run(const Window &window, cl::CommandQueue &queue)
{
    ARM_COMPUTE_ERROR_ON_UNCONFIGURED_KERNEL(this);
    ARM_COMPUTE_ERROR_ON_INVALID_SUBWINDOW(ICLKernel::window(), window);

    Window slice = window.first_slice_window_2D();

    do
    {
        unsigned int idx = 0;
        add_2D_tensor_argument(idx, _input, slice);
        add_2D_tensor_argument(idx, _output, slice);
        enqueue(queue, *this, slice);
    }
    while(window.slide_window_slice_2D(slice));
}
