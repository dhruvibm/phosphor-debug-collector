// SPDX-License-Identifier: Apache-2.0
#include "core_dump_retry.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <utility>

namespace phosphor::dump::core
{
namespace
{
constexpr size_t maxTimerAttempts = 3;

constexpr std::array<std::string_view, 7> transientDBusErrors = {
    "org.freedesktop.DBus.Error.Disconnected",
    "org.freedesktop.DBus.Error.NameHasNoOwner",
    "org.freedesktop.DBus.Error.NoReply",
    "org.freedesktop.DBus.Error.NoServer",
    "org.freedesktop.DBus.Error.ServiceUnknown",
    "org.freedesktop.DBus.Error.TimedOut",
    "org.freedesktop.DBus.Error.Timeout",
};
} // namespace

bool isTransientDBusError(std::string_view errorName)
{
    return std::ranges::find(transientDBusErrors, errorName) !=
           transientDBusErrors.end();
}

bool isCoreDumpFile(const std::filesystem::path& path)
{
    return path.filename().string().starts_with("core.");
}

RetryQueue::RetryQueue(Request request, SetTimer setTimer) :
    request(std::move(request)), setTimer(std::move(setTimer))
{}

void RetryQueue::enqueue(const std::filesystem::path& path)
{
    if (path.empty())
    {
        return;
    }

    auto [entry, inserted] = requests.try_emplace(path);
    if (!inserted)
    {
        return;
    }

    attempt(entry, false);
    updateTimer();
}

void RetryQueue::managerAvailable()
{
    for (auto& entry : requests)
    {
        entry.second.timerAttempts = 0;
    }

    for (auto entry = requests.begin(); entry != requests.end();)
    {
        entry = attempt(entry, false);
    }

    updateTimer();
}

void RetryQueue::timerExpired()
{
    // A one-shot timer is no longer armed when its callback runs.
    timerArmed = false;

    for (auto entry = requests.begin(); entry != requests.end();)
    {
        if (entry->second.timerAttempts >= maxTimerAttempts)
        {
            ++entry;
            continue;
        }

        entry = attempt(entry, true);
    }

    updateTimer();
}

size_t RetryQueue::pendingCount() const noexcept
{
    return requests.size();
}

RetryQueue::Requests::iterator RetryQueue::attempt(Requests::iterator entry,
                                                   bool timerAttempt)
{
    auto next = std::next(entry);
    auto result = request(entry->first);

    if (result == RequestResult::transientFailure)
    {
        if (timerAttempt)
        {
            ++entry->second.timerAttempts;
        }
        return next;
    }

    requests.erase(entry);
    return next;
}

void RetryQueue::updateTimer()
{
    auto needsTimer = std::ranges::any_of(requests, [](const auto& entry) {
        return entry.second.timerAttempts < maxTimerAttempts;
    });

    if (needsTimer == timerArmed)
    {
        return;
    }

    timerArmed = needsTimer;
    setTimer(timerArmed);
}

} // namespace phosphor::dump::core
