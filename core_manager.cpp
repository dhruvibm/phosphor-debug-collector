#include "config.h"

#include "core_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/exception.hpp>

#include <chrono>
#include <exception>
#include <filesystem>
#include <string>

namespace phosphor::dump::core
{
namespace
{
constexpr auto dumpCreateInterface = "xyz.openbmc_project.Dump.Create";
constexpr auto fallbackRetryInterval = std::chrono::seconds(30);
} // namespace

Manager::Manager(sdbusplus::bus_t& bus, const EventPtr& event) :
    bus(bus),
    retryTimer(event.get(), [this](auto&) { retryQueue.timerExpired(); }),
    retryQueue([this](const auto& file) { return createDump(file); },
               [this](bool enable) {
                   if (enable)
                   {
                       retryTimer.restartOnce(fallbackRetryInterval);
                   }
                   else
                   {
                       retryTimer.setEnabled(false);
                   }
               }),
    dumpManagerOwnerMatch(
        bus, sdbusplus::match_rules::nameOwnerChanged(DUMP_BUSNAME),
        [this](sdbusplus::message_t& msg) { handleNameOwnerChanged(msg); }),
    coreWatch(event, IN_NONBLOCK, coreFileEvent, EPOLLIN, CORE_FILE_DIR,
              [this](const auto& files) { watchCallback(files); })
{}

void Manager::watchCallback(const UserMap& fileInfo)
{
    for (const auto& [path, event] : fileInfo)
    {
        static_cast<void>(event);
        if (isCoreDumpFile(path))
        {
            retryQueue.enqueue(path);
        }
    }
}

void Manager::handleNameOwnerChanged(sdbusplus::message_t& msg)
{
    try
    {
        std::string name;
        std::string oldOwner;
        std::string newOwner;
        msg.read(name, oldOwner, newOwner);

        if (name != DUMP_BUSNAME || newOwner.empty() ||
            retryQueue.pendingCount() == 0)
        {
            return;
        }

        lg2::info("Dump Manager is available; retrying {COUNT} core dumps",
                  "COUNT", retryQueue.pendingCount());
        retryQueue.managerAvailable();
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to process Dump Manager owner change: {ERROR}",
                   "ERROR", e);
    }
}

RequestResult Manager::createDump(const std::filesystem::path& file)
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file, ec))
    {
        lg2::warning("Core file is no longer available: {FILE}", "FILE", file);
        return RequestResult::permanentFailure;
    }

    try
    {
        auto method = bus.new_method_call(DUMP_BUSNAME, BMC_DUMP_OBJPATH,
                                          dumpCreateInterface, "CreateDump");

        phosphor::dump::DumpCreateParams params;
        using CreateParameters = sdbusplus::common::xyz::openbmc_project::dump::
            Create::CreateParameters;
        using DumpType =
            sdbusplus::common::xyz::openbmc_project::dump::Create::DumpType;
        using DumpInterface =
            sdbusplus::common::xyz::openbmc_project::dump::Create;

        params[DumpInterface::convertCreateParametersToString(
            CreateParameters::DumpType)] =
            DumpInterface::convertDumpTypeToString(DumpType::ApplicationCored);
        params[DumpInterface::convertCreateParametersToString(
            CreateParameters::FilePath)] = file.string();
        method.append(params);

        auto response = bus.call(method);
        sdbusplus::object_path entry;
        response.read(entry);
        lg2::info("Created core dump request {ENTRY} for {FILE}", "ENTRY",
                  entry, "FILE", file);
        return RequestResult::success;
    }
    catch (const sdbusplus::exception_t& e)
    {
        if (isTransientDBusError(e.name()))
        {
            lg2::warning(
                "Dump Manager is unavailable for {FILE}: {ERROR}; queued for "
                "retry",
                "FILE", file, "ERROR", e);
            return RequestResult::transientFailure;
        }

        lg2::error(
            "Core dump request for {FILE} was rejected: {ERROR}; not retrying",
            "FILE", file, "ERROR", e);
        return RequestResult::permanentFailure;
    }
}

} // namespace phosphor::dump::core
