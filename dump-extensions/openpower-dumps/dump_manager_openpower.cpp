#include "dump_manager_openpower.hpp"

#include "dump_entry_factory.hpp"
#include "op_dump_consts.hpp"
#include "op_dump_util.hpp"
#include "openpower_dump_entry.hpp"

#include <phosphor-logging/lg2.hpp>

#include <charconv>
#include <format>
#include <optional>
#include <regex>

namespace openpower::dump
{

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

namespace
{
struct DumpFileDetails
{
    uint32_t id;
    uint64_t timestamp;
    uint64_t size;
    std::filesystem::path path;
};

std::optional<uint32_t> parseEntryId(std::string_view idString)
{
    static const std::regex idPattern(R"([0-9a-fA-F]{8})");
    if (!std::regex_match(idString.begin(), idString.end(), idPattern))
    {
        return std::nullopt;
    }

    uint32_t id = 0;
    const auto [end, error] = std::from_chars(
        idString.data(), idString.data() + idString.size(), id, 16);
    if (error != std::errc{} || end != idString.data() + idString.size())
    {
        return std::nullopt;
    }
    return id;
}

std::optional<DumpFileDetails> parseDumpFile(
    const std::filesystem::path& fullPath)
{
    const std::string filename = fullPath.filename().string();

    // Parse filename SYSDUMP.<SerialNumber>.<DumpId>.<DateTime>.
    static const std::regex pattern(
        R"(SYSDUMP\.[a-zA-Z0-9]+\.([0-9a-fA-F]{8})\.([0-9]{14}))");
    std::smatch match;
    if (!std::regex_match(filename, match, pattern))
    {
        lg2::error("Filename does not match expected format, {FILENAME}",
                   "FILENAME", filename);
        return std::nullopt;
    }

    auto dumpId = parseEntryId(match[1].str());
    if (!dumpId.has_value())
    {
        lg2::error("Failed to parse dump ID from {FILENAME}", "FILENAME",
                   filename);
        return std::nullopt;
    }

    std::error_code fileError;
    const uint64_t fileSize = std::filesystem::file_size(fullPath, fileError);
    if (fileError)
    {
        lg2::error("Failed to read dump file size for {PATH}: {ERROR}", "PATH",
                   fullPath, "ERROR", fileError.message());
        return std::nullopt;
    }

    return DumpFileDetails{dumpId.value(), util::timeToEpoch(match[2].str()),
                           fileSize, fullPath};
}

std::optional<DumpFileDetails> findDumpFile(
    const std::filesystem::path& entryPath, uint32_t expectedId)
{
    std::optional<DumpFileDetails> result;
    std::error_code error;
    std::filesystem::directory_iterator iterator(entryPath, error);
    const std::filesystem::directory_iterator end;

    while (!error && iterator != end)
    {
        const auto path = iterator->path();
        if (path.filename() != phosphor::dump::PRESERVE)
        {
            std::error_code typeError;
            if (iterator->is_regular_file(typeError) && !typeError)
            {
                auto details = parseDumpFile(path);
                if (details.has_value() && details->id == expectedId)
                {
                    if (result.has_value())
                    {
                        lg2::error(
                            "Multiple dump files found for entry {DUMP_ID}",
                            "DUMP_ID", std::format("{:08X}", expectedId));
                        return std::nullopt;
                    }
                    result = std::move(details);
                }
                else if (details.has_value())
                {
                    lg2::error(
                        "Filename ID {FILE_ID} does not match entry {DUMP_ID}",
                        "FILE_ID", std::format("{:08X}", details->id),
                        "DUMP_ID", std::format("{:08X}", expectedId));
                }
            }
        }
        iterator.increment(error);
    }

    if (error)
    {
        lg2::error("Failed to inspect dump directory {PATH}: {ERROR}", "PATH",
                   entryPath, "ERROR", error.message());
        return std::nullopt;
    }
    return result;
}
} // namespace

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
    auto details = parseDumpFile(fullPath);
    if (!details.has_value())
    {
        return;
    }

    auto it = entries.find(details->id);
    if (it == entries.end())
    {
        lg2::error("Entry with Dump ID {DUMP_ID} not found", "DUMP_ID",
                   std::format("{:08X}", details->id));
        return;
    }
    auto* opEntry = dynamic_cast<openpower::dump::Entry*>(it->second.get());
    if (opEntry == nullptr)
    {
        lg2::error("Entry with Dump ID {DUMP_ID} is not an OpenPOWER entry",
                   "DUMP_ID", std::format("{:08X}", details->id));
        return;
    }

    opEntry->update(details->timestamp, details->size, details->path);
}

void Manager::restore()
{
    const std::filesystem::path dumpPath(OP_DUMP_PATH);
    std::error_code error;
    if (!std::filesystem::is_directory(dumpPath, error))
    {
        if (error)
        {
            lg2::error("Failed to inspect dump directory {PATH}: {ERROR}",
                       "PATH", dumpPath, "ERROR", error.message());
        }
        return;
    }

    DumpEntryFactory dumpFactory(bus, baseEntryPath, *this);
    std::filesystem::directory_iterator iterator(dumpPath, error);
    const std::filesystem::directory_iterator end;

    while (!error && iterator != end)
    {
        std::error_code typeError;
        if (iterator->is_directory(typeError) && !typeError)
        {
            const auto entryPath = iterator->path();
            const auto idString = entryPath.filename().string();
            auto id = parseEntryId(idString);
            if (!id.has_value())
            {
                lg2::warning("Ignoring invalid dump directory {PATH}", "PATH",
                             entryPath);
                iterator.increment(error);
                continue;
            }

            lastEntryId =
                std::max(lastEntryId, id.value() & DUMP_ID_VALUE_MASK);

            auto details = findDumpFile(entryPath, id.value());
            if (!details.has_value())
            {
                iterator.increment(error);
                continue;
            }

            const auto objectPath = std::filesystem::path(baseEntryPath) /
                                    std::format("{:08X}", id.value());
            auto entry =
                dumpFactory.createEntryForRestore(id.value(), objectPath);
            if (!entry)
            {
                iterator.increment(error);
                continue;
            }

            entry->deserialize(entryPath);
            auto* opEntry = dynamic_cast<openpower::dump::Entry*>(entry.get());
            if (opEntry == nullptr)
            {
                lg2::error("Restored entry {DUMP_ID} is not OpenPOWER",
                           "DUMP_ID", std::format("{:08X}", id.value()));
                iterator.increment(error);
                continue;
            }

            opEntry->restoreFile(details->timestamp, details->size,
                                 details->path);
            entries.try_emplace(id.value(), std::move(entry));
        }
        iterator.increment(error);
    }

    if (error)
    {
        lg2::error("Failed to restore dumps from {PATH}: {ERROR}", "PATH",
                   dumpPath, "ERROR", error.message());
    }
}

} // namespace openpower::dump
