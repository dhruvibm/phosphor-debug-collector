// SPDX-License-Identifier: Apache-2.0
#include "core_dump_retry.hpp"

#include <cstddef>
#include <filesystem>
#include <vector>

#include <gtest/gtest.h>

namespace phosphor::dump::core
{
namespace
{

TEST(CoreDumpRetry, IgnoresEmptyPath)
{
    size_t requests = 0;
    RetryQueue queue(
        [&requests](const auto&) {
            ++requests;
            return RequestResult::success;
        },
        [](bool) {});

    queue.enqueue({});

    EXPECT_EQ(requests, 0);
    EXPECT_EQ(queue.pendingCount(), 0);
}

TEST(CoreDumpRetry, RecognizesOnlyCoreFileNames)
{
    EXPECT_TRUE(isCoreDumpFile("/tmp/core.application.123.zst"));
    EXPECT_FALSE(isCoreDumpFile("/tmp/systemd-private-core.tmp"));
    EXPECT_FALSE(isCoreDumpFile("/tmp/core"));
}

TEST(CoreDumpRetry, DeduplicatesPendingPath)
{
    size_t requests = 0;
    std::vector<bool> timerChanges;
    RetryQueue queue(
        [&requests](const auto&) {
            ++requests;
            return RequestResult::transientFailure;
        },
        [&timerChanges](bool enabled) { timerChanges.push_back(enabled); });

    queue.enqueue("/tmp/core.application.1");
    queue.enqueue("/tmp/core.application.1");

    EXPECT_EQ(requests, 1);
    EXPECT_EQ(queue.pendingCount(), 1);
    EXPECT_EQ(timerChanges, std::vector<bool>({true}));
}

TEST(CoreDumpRetry, DoesNotRetryPermanentFailure)
{
    size_t requests = 0;
    RetryQueue queue(
        [&requests](const auto&) {
            ++requests;
            return RequestResult::permanentFailure;
        },
        [](bool) {});

    queue.enqueue("/tmp/core.application.2");
    queue.timerExpired();
    queue.managerAvailable();

    EXPECT_EQ(requests, 1);
    EXPECT_EQ(queue.pendingCount(), 0);
}

TEST(CoreDumpRetry, BoundsFallbackTimerAttempts)
{
    size_t requests = 0;
    RetryQueue queue(
        [&requests](const auto&) {
            ++requests;
            return RequestResult::transientFailure;
        },
        [](bool) {});

    queue.enqueue("/tmp/core.application.3");
    for (size_t i = 0; i < 10; ++i)
    {
        queue.timerExpired();
    }

    // One initial request and three bounded timer retries.
    EXPECT_EQ(requests, 4);
    EXPECT_EQ(queue.pendingCount(), 1);
}

TEST(CoreDumpRetry, ManagerRestartRetriesAfterTimerLimit)
{
    size_t requests = 0;
    RetryQueue queue(
        [&requests](const auto&) {
            ++requests;
            return requests == 5 ? RequestResult::success
                                 : RequestResult::transientFailure;
        },
        [](bool) {});

    queue.enqueue("/tmp/core.application.4");
    for (size_t i = 0; i < 3; ++i)
    {
        queue.timerExpired();
    }

    queue.managerAvailable();

    EXPECT_EQ(requests, 5);
    EXPECT_EQ(queue.pendingCount(), 0);
}

TEST(CoreDumpRetry, SuccessfulRestartCancelsFallbackTimer)
{
    size_t requests = 0;
    std::vector<bool> timerChanges;
    RetryQueue queue(
        [&requests](const auto&) {
            ++requests;
            return requests == 1 ? RequestResult::transientFailure
                                 : RequestResult::success;
        },
        [&timerChanges](bool enabled) { timerChanges.push_back(enabled); });

    queue.enqueue("/tmp/core.application.5");
    queue.managerAvailable();

    EXPECT_EQ(requests, 2);
    EXPECT_EQ(queue.pendingCount(), 0);
    EXPECT_EQ(timerChanges, std::vector<bool>({true, false}));
}

TEST(CoreDumpRetry, FailureDoesNotBlockAnotherCore)
{
    std::vector<std::filesystem::path> requests;
    RetryQueue queue(
        [&requests](const auto& path) {
            requests.push_back(path);
            if (path.filename() == "core.bad.1")
            {
                return RequestResult::transientFailure;
            }
            return RequestResult::success;
        },
        [](bool) {});

    queue.enqueue("/tmp/core.bad.1");
    queue.enqueue("/tmp/core.good.2");

    EXPECT_EQ(requests.size(), 2);
    EXPECT_EQ(queue.pendingCount(), 1);
}

TEST(CoreDumpRetry, ClassifiesOnlyAvailabilityErrorsAsTransient)
{
    EXPECT_TRUE(isTransientDBusError("org.freedesktop.DBus.Error.NoReply"));
    EXPECT_TRUE(
        isTransientDBusError("org.freedesktop.DBus.Error.ServiceUnknown"));
    EXPECT_FALSE(
        isTransientDBusError("xyz.openbmc_project.Dump.Error.QuotaExceeded"));
    EXPECT_FALSE(isTransientDBusError(
        "xyz.openbmc_project.Common.Error.InvalidArgument"));
}

} // namespace
} // namespace phosphor::dump::core
