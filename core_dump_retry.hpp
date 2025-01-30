// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <string_view>

namespace phosphor::dump::core
{

/** @brief Result of requesting a core dump entry. */
enum class RequestResult
{
    success,
    transientFailure,
    permanentFailure,
};

/** @brief Return true when a D-Bus error may succeed after a restart. */
bool isTransientDBusError(std::string_view errorName);

/** @brief Return true for a completed systemd core file name. */
bool isCoreDumpFile(const std::filesystem::path& path);

/** @class RetryQueue
 *  @brief Deduplicates core dump requests and bounds timer retries.
 */
class RetryQueue
{
  public:
    using Request =
        std::function<RequestResult(const std::filesystem::path& path)>;
    using SetTimer = std::function<void(bool enabled)>;

    RetryQueue(Request request, SetTimer setTimer);

    RetryQueue() = delete;
    RetryQueue(const RetryQueue&) = delete;
    RetryQueue& operator=(const RetryQueue&) = delete;
    RetryQueue(RetryQueue&&) = delete;
    RetryQueue& operator=(RetryQueue&&) = delete;
    ~RetryQueue() = default;

    /** @brief Add and immediately attempt a new core dump request. */
    void enqueue(const std::filesystem::path& path);

    /** @brief Retry all requests after Dump Manager acquires its name. */
    void managerAvailable();

    /** @brief Process one bounded timer retry for eligible requests. */
    void timerExpired();

    /** @brief Return the number of requests waiting for recovery. */
    size_t pendingCount() const noexcept;

  private:
    struct PendingRequest
    {
        size_t timerAttempts = 0;
    };

    using Requests = std::map<std::filesystem::path, PendingRequest>;

    /** @brief Attempt one request and return the next map iterator. */
    Requests::iterator attempt(Requests::iterator request, bool timerAttempt);

    /** @brief Arm or cancel the fallback timer as needed. */
    void updateTimer();

    Request request;
    SetTimer setTimer;
    Requests requests;
    bool timerArmed = false;
};

} // namespace phosphor::dump::core
