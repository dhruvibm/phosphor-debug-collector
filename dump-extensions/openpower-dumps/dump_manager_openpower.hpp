#pragma once

#include "dump_manager.hpp"
#include "dump_utils.hpp"
#include "watch.hpp"

#include <sys/epoll.h>
#include <sys/inotify.h>

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Dump/Create/server.hpp>

#include <filesystem>
#include <map>
#include <memory>

namespace openpower::dump
{

using OpDumpIfaces = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Dump::server::Create>;
using UserMap = phosphor::dump::inotify::UserMap;
using Watch = phosphor::dump::inotify::Watch;

/** @class Manager
 *  @brief OpenPOWER dump manager implementation.
 *  @details Implements the xyz.openbmc_project.Dump.Create D-Bus API and
 *  completes entries when their files are written to BMC storage.
 */
class Manager :
    virtual public OpDumpIfaces,
    virtual public phosphor::dump::Manager
{
  public:
    Manager() = delete;
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) = delete;
    Manager& operator=(Manager&&) = delete;
    virtual ~Manager() = default;

    Manager(sdbusplus::bus_t& bus, const phosphor::dump::EventPtr& event,
            const char* path, const std::string& baseEntryPath,
            const std::filesystem::path& filePath) :
        OpDumpIfaces(bus, path),
        phosphor::dump::Manager(bus, path, baseEntryPath),
        eventLoop(sd_event_ref(event.get())),
        dumpWatch(
            eventLoop, IN_NONBLOCK, IN_CLOSE_WRITE | IN_CREATE | IN_MOVED_TO,
            EPOLLIN, filePath, [this](const UserMap& fileInfo) {
                for (const auto& [path, eventMask] : fileInfo)
                {
                    if ((eventMask & (IN_CLOSE_WRITE | IN_MOVED_TO)) != 0U &&
                        !std::filesystem::is_directory(path))
                    {
                        updateEntry(path);
                    }
                    else if ((eventMask & IN_CREATE) != 0U &&
                             std::filesystem::is_directory(path))
                    {
                        auto recursiveWatch = std::make_unique<Watch>(
                            eventLoop, IN_NONBLOCK,
                            IN_CLOSE_WRITE | IN_MOVED_TO, EPOLLIN, path,
                            [this](const UserMap& recursiveFileInfo) {
                                for (const auto& [recursivePath,
                                                  recursiveEventMask] :
                                     recursiveFileInfo)
                                {
                                    if ((recursiveEventMask &
                                         (IN_CLOSE_WRITE | IN_MOVED_TO)) !=
                                            0U &&
                                        !std::filesystem::is_directory(
                                            recursivePath))
                                    {
                                        updateEntry(recursivePath);
                                    }
                                }
                            });
                        childWatchMap.try_emplace(path,
                                                  std::move(recursiveWatch));
                    }
                }
            })
    {}

    void restore() override
    {
        // TODO: Restore serialized OpenPOWER entries.
    }

    sdbusplus::object_path createDump(
        phosphor::dump::DumpCreateParams params) override;

  private:
    void updateEntry(const std::filesystem::path& fullPath);

    /** @brief Event loop used by the root and child directory watches. */
    phosphor::dump::EventPtr eventLoop;

    /** @brief Watch for dump files and per-entry dump directories. */
    Watch dumpWatch;

    /** @brief Watches for files written inside per-entry directories. */
    std::map<std::filesystem::path, std::unique_ptr<Watch>> childWatchMap;
};

} // namespace openpower::dump
