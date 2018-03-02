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
#include "arm_compute/graph/INode.h"

#include "arm_compute/core/CL/OpenCL.h"
#include "arm_compute/core/Validate.h"

using namespace arm_compute::graph;

TargetHint INode::override_target_hint(TargetHint target_hint) const
{
    if(target_hint == TargetHint::OPENCL && !opencl_is_available())
    {
        target_hint = TargetHint::DONT_CARE;
    }
    GraphHints hints{ target_hint };
    target_hint = node_override_hints(hints).target_hint();
    ARM_COMPUTE_ERROR_ON(target_hint == TargetHint::OPENCL && !opencl_is_available());
    return target_hint;
}
bool INode::supports_in_place() const
{
    return _supports_in_place;
}
void INode::set_supports_in_place(bool value)
{
    _supports_in_place = value;
}
GraphHints INode::node_override_hints(GraphHints hints) const
{
    TargetHint target_hint = hints.target_hint();
    hints.set_target_hint((target_hint == TargetHint::DONT_CARE) ? TargetHint::NEON : target_hint);
    return hints;
}
