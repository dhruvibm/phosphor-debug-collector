#include "dump_manager_openpower.hpp"

#include "dump_entry_factory.hpp"

#include <phosphor-logging/lg2.hpp>

#include <format>

namespace openpower::dump
{

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

sdbusplus::object_path Manager::createDump(
    phosphor::dump::DumpCreateParams params)
{
    try
    {
        DumpEntryFactory dumpFactory(bus, baseEntryPath, *this);

        auto dumpEntry = dumpFactory.createEntry(lastEntryId + 1, params);
        if (!dumpEntry)
        {
            lg2::error("Dump entry creation failed");
            return {};
        }

        uint32_t id = dumpEntry->getDumpId();
        entries.insert(std::make_pair(id, std::move(dumpEntry)));
        std::string idStr = std::format("{:08X}", id);
        lastEntryId++;
        return baseEntryPath + "/" + idStr;
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to create dump: {ERROR}", "ERROR", e);
        throw;
    }
}

} // namespace openpower::dump
