// SPDX-License-Identifier: Apache-2.0
#include "dump-extensions/openpower-dumps/op_dump_policy.hpp"

#include <gtest/gtest.h>

namespace openpower::dump
{
namespace
{

TEST(OpenPowerDumpPolicy, DoesNotControlUserInitiatedDumps)
{
    EXPECT_FALSE(isDumpPolicyApplicable(OpDumpTypes::System));
    EXPECT_FALSE(isDumpPolicyApplicable(OpDumpTypes::Resource));
}

TEST(OpenPowerDumpPolicy, ControlsAutomaticDumps)
{
    EXPECT_TRUE(isDumpPolicyApplicable(OpDumpTypes::Hardware));
    EXPECT_TRUE(isDumpPolicyApplicable(OpDumpTypes::Hostboot));
    EXPECT_TRUE(isDumpPolicyApplicable(OpDumpTypes::SBE));
    EXPECT_TRUE(isDumpPolicyApplicable(OpDumpTypes::MemoryBufferSBE));
}

} // namespace
} // namespace openpower::dump
