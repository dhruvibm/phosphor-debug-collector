#pragma once

#include "config.h"

#include "core_dump_retry.hpp"
#include "dump_utils.hpp"
#include "watch.hpp"

#include <sdbusplus/bus/match.hpp>
#include <sdeventplus/utility/timer.hpp>

#include <filesystem>

namespace phosphor::dump::core
{

using Watch = phosphor::dump::inotify::Watch;
using UserMap = phosphor::dump::inotify::UserMap;

/** workaround: Watches for IN_CLOSE_WRITE event for the
 *  jffs filesystem based systemd-coredump core path
 *  Refer openbmc/issues/#2287 for more details.
 *
 *  JFFS_CORE_FILE_WORKAROUND will be enabled for jffs and
 *  for other file system it will be disabled.
 */
#ifdef JFFS_CORE_FILE_WORKAROUND
static constexpr auto coreFileEvent = IN_CLOSE_WRITE;
#else
static constexpr auto coreFileEvent = IN_CREATE;
#endif

/** @class Manager
 *  @brief OpenBMC core dump monitor implementation.
 */
class Manager
{
  public:
    Manager() = delete;
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) = delete;
    Manager& operator=(Manager&&) = delete;
    virtual ~Manager() = default;

    /** @brief Construct the core file and Dump Manager monitors. */
    Manager(sdbusplus::bus_t& bus, const EventPtr& event);

  private:
    using RetryTimer =
        sdeventplus::utility::Timer<sdeventplus::ClockId::Monotonic>;

    /** @brief Request one ApplicationCored dump entry. */
    RequestResult createDump(const std::filesystem::path& file);

    /** @brief Enqueue valid core files from an inotify callback. */
    void watchCallback(const UserMap& fileInfo);

    /** @brief Retry pending requests when Dump Manager starts. */
    void handleNameOwnerChanged(sdbusplus::message_t& msg);

    sdbusplus::bus_t& bus;
    RetryTimer retryTimer;
    RetryQueue retryQueue;
    sdbusplus::match dumpManagerOwnerMatch;
    Watch coreWatch;
};

} // namespace phosphor::dump::core
