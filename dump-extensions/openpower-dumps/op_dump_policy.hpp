// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <com/ibm/Dump/Create/common.hpp>

namespace openpower::dump
{

using OpDumpTypes = sdbusplus::common::com::ibm::dump::Create::DumpType;

/** @brief Return whether the automatic dump policy controls a dump type. */
constexpr bool isDumpPolicyApplicable(OpDumpTypes type) noexcept
{
    switch (type)
    {
        case OpDumpTypes::System:
        case OpDumpTypes::Resource:
            return false;
        default:
            return true;
    }
}

} // namespace openpower::dump
