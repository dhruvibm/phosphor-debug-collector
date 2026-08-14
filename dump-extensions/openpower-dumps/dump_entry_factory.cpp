#include "dump_entry_factory.hpp"

#include "op_dump_util.hpp"
#include "openpower_dump_entry.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <algorithm>
#include <cctype>
#include <format>
#include <string_view>

namespace openpower::dump
{

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

namespace
{
bool createsNonDisruptiveSystemDump(const std::optional<std::string>& vspString)
{
    if (!vspString.has_value() || vspString->empty())
    {
        return true;
    }

    constexpr std::string_view systemSelector = "SYSTEM";
    return vspString->size() == systemSelector.size() &&
           std::equal(vspString->begin(), vspString->end(),
                      systemSelector.begin(), [](char input, char expected) {
                          return std::toupper(static_cast<unsigned char>(
                                     input)) == expected;
                      });
}
} // namespace

std::unique_ptr<phosphor::dump::Entry> DumpEntryFactory::createSystemDumpEntry(
    uint32_t id, const std::filesystem::path& objPath, uint64_t timeStamp,
    const DumpParameters& dumpParams)
{
    using Unavailable =
        sdbusplus::xyz::openbmc_project::Common::Error::Unavailable;

    if (openpower::dump::util::isSystemDumpInProgress(bus))
    {
        lg2::error("Another dump in progress or available to offload");
        elog<Unavailable>();
    }

    using NotAllowed =
        sdbusplus::xyz::openbmc_project::Common::Error::NotAllowed;
    using Reason = xyz::openbmc_project::Common::NotAllowed::REASON;

    auto isHostRunning = false;
    phosphor::dump::HostState hostState;
    try
    {
        isHostRunning = phosphor::dump::isHostRunning();
        hostState = phosphor::dump::getHostState();
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "System state cannot be determined, system dump is not allowed: "
            "{ERROR}",
            "ERROR", e);
        elog<NotAllowed>(Reason("System dump not allowed currently."));
    }
    bool isHostQuiesced = hostState == phosphor::dump::HostState::Quiesced;
    bool isHostTransitioningToOff =
        hostState == phosphor::dump::HostState::TransitioningToOff;
    // Allow creating system dump only when the host is up or quiesced
    // starting to power off
    if (!isHostRunning && !isHostQuiesced && !isHostTransitioningToOff)
    {
        lg2::error("System dump can be initiated only when the host is up "
                   "or quiesced or starting to poweroff");
        elog<NotAllowed>(Reason(
            "System dump can be initiated only when the host is up "
            "or quiesced or starting to poweroff"));
    }

    return std::make_unique<system::Entry>(
        bus, objPath.c_str(), id, timeStamp, 0,
        phosphor::dump::OperationStatus::InProgress, dumpParams.originatorId,
        dumpParams.originatorType, mgr);
}

std::unique_ptr<phosphor::dump::Entry>
    DumpEntryFactory::createResourceDumpEntry(
        uint32_t id, const std::filesystem::path& objPath, uint64_t timeStamp,
        const DumpParameters& dumpParams)
{
    using NotAllowed =
        sdbusplus::xyz::openbmc_project::Common::Error::NotAllowed;
    using Reason = xyz::openbmc_project::Common::NotAllowed::REASON;

    if (!phosphor::dump::isHostRunning())
    {
        elog<NotAllowed>(
            Reason("Resource dump can be initiated only when the host is up"));
    }

    if (createsNonDisruptiveSystemDump(dumpParams.vspString))
    {
        return std::make_unique<system::Entry>(
            bus, objPath.c_str(), id, timeStamp, 0,
            phosphor::dump::OperationStatus::InProgress,
            dumpParams.originatorId, dumpParams.originatorType,
            system::SystemImpact::NonDisruptive,
            dumpParams.userChallenge.value_or(""), mgr);
    }

    return std::make_unique<resource::Entry>(
        bus, objPath.c_str(), id, timeStamp, 0,
        dumpParams.vspString.value_or(""),
        dumpParams.userChallenge.value_or(""),
        phosphor::dump::OperationStatus::InProgress, dumpParams.originatorId,
        dumpParams.originatorType, mgr);
}

std::unique_ptr<phosphor::dump::Entry> DumpEntryFactory::createEntry(
    uint32_t id, const phosphor::dump::DumpCreateParams& params)
{
    DumpParameters dumpParams = util::extractDumpParameters(params);

    std::string idStr = std::format("{:08X}", id);

    auto objPath = std::filesystem::path(baseEntryPath) / idStr;

    uint64_t timeStamp =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();

    switch (dumpParams.type)
    {
        case OpDumpTypes::System:
            return createSystemDumpEntry(id, objPath, timeStamp, dumpParams);
        case OpDumpTypes::Resource:
            return createResourceDumpEntry(id, objPath, timeStamp, dumpParams);
        default:
            util::throwInvalidArgument("DUMP_TYPE_NOT_VALID", "INVALID_INPUT");
    }
    return nullptr;
}

} // namespace openpower::dump
