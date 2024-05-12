#include "dump_manager_openpower.hpp"

#include "dump_entry_factory.hpp"
#include "op_dump_util.hpp"
#include "openpower_dump_entry.hpp"

#include <phosphor-logging/lg2.hpp>

#include <charconv>
#include <format>
#include <regex>

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

void Manager::updateEntry(const std::filesystem::path& fullPath)
{
    lg2::info("A new dump file found {PATH}", "PATH", fullPath.string());
    const std::string filename = fullPath.filename().string();

    // Parse Filename SYSDUMP.<SerialNumber>.<DumpId>.<DateTime>Date
    static const std::regex pattern(
        R"(SYSDUMP\.[a-zA-Z0-9]+\.([0-9a-fA-F]{8})\.([0-9]{14}))");
    std::smatch match;

    if (!std::regex_match(filename, match, pattern))
    {
        lg2::error("Filename does not match expected format, {FILENAME}",
                   "FILENAME", filename);
        return;
    }

    const std::string dumpIdString = match[1];
    const std::string timestampString = match[2];

    uint32_t dumpId = 0;
    const auto [end, error] =
        std::from_chars(dumpIdString.data(),
                        dumpIdString.data() + dumpIdString.size(), dumpId, 16);
    if (error != std::errc{} ||
        end != dumpIdString.data() + dumpIdString.size())
    {
        lg2::error("Failed to parse dump ID from {FILENAME}", "FILENAME",
                   filename);
        return;
    }

    std::error_code fileError;
    const uint64_t fileSize = std::filesystem::file_size(fullPath, fileError);
    if (fileError)
    {
        lg2::error("Failed to read dump file size for {PATH}: {ERROR}", "PATH",
                   fullPath, "ERROR", fileError.message());
        return;
    }

    const uint64_t timestamp = util::timeToEpoch(timestampString);

    auto it = entries.find(dumpId);
    if (it == entries.end())
    {
        lg2::error("Entry with Dump ID {DUMP_ID} not found", "DUMP_ID",
                   std::format("{:08X}", dumpId));
        return;
    }
    auto* opEntry = dynamic_cast<openpower::dump::Entry*>(it->second.get());
    if (opEntry == nullptr)
    {
        lg2::error("Entry with Dump ID {DUMP_ID} is not an OpenPOWER entry",
                   "DUMP_ID", std::format("{:08X}", dumpId));
        return;
    }

    opEntry->update(timestamp, fileSize, fullPath);
}

} // namespace openpower::dump
