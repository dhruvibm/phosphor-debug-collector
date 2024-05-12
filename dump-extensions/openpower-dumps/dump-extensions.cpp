#include "dump-extensions.hpp"

#include "dump_manager_openpower.hpp"
#include "dump_utils.hpp"
#include "op_dump_consts.hpp"
#include "xyz/openbmc_project/Common/error.hpp"

#include <xyz/openbmc_project/Dump/Create/client.hpp>

namespace phosphor::dump
{

void loadExtensions(sdbusplus::bus_t& bus, DumpManagerList& dumpList)
{
    using namespace phosphor::logging;
    using InternalFailure =
        sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
    sd_event* event = nullptr;
    auto rc = sd_event_default(&event);
    if (rc < 0)
    {
        lg2::error("Error occurred during the sd_event_default, rc: {RC}", "RC",
                   rc);
        elog<InternalFailure>();
    }
    EventPtr eventP{event};
    event = nullptr;

    try
    {
        std::filesystem::create_directories(openpower::dump::OP_DUMP_PATH);
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to create OpenPOWER dump directory {PATH}: "
                   "{ERROR}",
                   "PATH", openpower::dump::OP_DUMP_PATH, "ERROR", e);
        throw std::runtime_error("Failed to create OpenPOWER dump directory");
    }

    using DumpCreate = sdbusplus::client::xyz::openbmc_project::dump::Create<>;
    auto opDumpPath =
        sdbusplus::object_path(DumpCreate::namespace_path::value) /
        DumpCreate::namespace_path::system;
    auto opDumpEntryPath = opDumpPath / "entry";

    dumpList.push_back(std::make_unique<openpower::dump::Manager>(
        bus, eventP, opDumpPath.str.c_str(), opDumpEntryPath,
        openpower::dump::OP_DUMP_PATH));
}
} // namespace phosphor::dump
