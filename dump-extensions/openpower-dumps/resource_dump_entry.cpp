#include "resource_dump_entry.hpp"

#include "dump_utils.hpp"
#include "host_transport_exts.hpp"
#include "op_dump_consts.hpp"

#include <fmt/core.h>

#include <phosphor-logging/elog-errors.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

namespace openpower
{
namespace dump
{
namespace resource
{
// TODO #ibm-openbmc/issues/2859
// Revisit host transport impelementation
// This value is used to identify the dump in the transport layer to host,
constexpr auto TRANSPORT_DUMP_TYPE_IDENTIFIER = 9;
using namespace phosphor::logging;

void Entry::initiateOffload(std::string uri)
{
    using NotAllowed =
        sdbusplus::xyz::openbmc_project::Common::Error::NotAllowed;
    using Reason = xyz::openbmc_project::Common::NotAllowed::REASON;
    log<level::INFO>(
        fmt::format(
            "Resource dump offload request id({}) uri({}) source dumpid({})",
            id, uri, sourceDumpId())
            .c_str());

    if (!phosphor::dump::isHostRunning())
    {
        elog<NotAllowed>(
            Reason("This dump can be offloaded only when the host is up"));
        return;
    }
    phosphor::dump::Entry::initiateOffload(uri);
    phosphor::dump::host::requestOffload(sourceDumpId());
}

void Entry::delete_()
{
    auto srcDumpID = sourceDumpId();
    auto dumpId = id;
    log<level::INFO>(fmt::format("Resource dump delete id({}) srcdumpid({})",
                                 dumpId, srcDumpID)
                         .c_str());
    // Remove Dump entry D-bus object
    phosphor::dump::Entry::delete_();

    // Remove resource dump when host is up by using source dump id
    // which is present in resource dump entry dbus object as a property.
    if ((phosphor::dump::isHostRunning()) && (srcDumpID != INVALID_SOURCE_ID))
    {
        phosphor::dump::host::requestDelete(srcDumpID,
                                            TRANSPORT_DUMP_TYPE_IDENTIFIER);
    }
}
} // namespace resource
} // namespace dump
} // namespace openpower
