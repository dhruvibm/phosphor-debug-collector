#include "openpower_dump_entry.hpp"

#include "dump_manager.hpp"
#include "dump_offload.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

#include <fstream>

namespace openpower::dump
{

namespace
{
std::filesystem::path getMetadataPath(const std::filesystem::path& dumpPath)
{
    return dumpPath / phosphor::dump::PRESERVE / OP_SERIAL_FILE;
}
} // namespace

void Entry::delete_()
{
    if (!file.empty())
    {
        try
        {
            std::filesystem::remove_all(file.parent_path());
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            lg2::error("Failed to delete dump file: {ERROR}", "ERROR", e);
        }
    }

    phosphor::dump::Entry::delete_();
}

void Entry::initiateOffload(std::string uri)
{
    phosphor::dump::offload::requestOffload(file, id, std::move(uri));
    offloaded(true);
}

void Entry::serialize()
{
    if (file.empty())
    {
        lg2::error("Cannot serialize OpenPOWER entry {DUMP_ID} without a file",
                   "DUMP_ID", id);
        return;
    }

    phosphor::dump::Entry::serialize();

    const auto metadataPath = getMetadataPath(file.parent_path());
    try
    {
        std::ofstream stream(metadataPath, std::ios::binary);
        if (!stream.is_open())
        {
            lg2::error("Failed to open OpenPOWER metadata {PATH}", "PATH",
                       metadataPath);
            return;
        }

        const auto metadata = getMetadata();
        nlohmann::json json{{"version", OP_SERIALIZATION_VERSION},
                            {"dumpId", id}};

        if (metadata.systemImpact.has_value())
        {
            json["systemImpact"] = metadata.systemImpact.value();
        }
        if (metadata.userChallenge.has_value())
        {
            json["userChallenge"] = metadata.userChallenge.value();
        }
        if (metadata.vspString.has_value())
        {
            json["vspString"] = metadata.vspString.value();
        }
        if (metadata.errorLogId.has_value())
        {
            json["errorLogId"] = metadata.errorLogId.value();
        }
        if (metadata.failingUnitId.has_value())
        {
            json["failingUnitId"] = metadata.failingUnitId.value();
        }

        stream << json.dump(4);
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to serialize OpenPOWER metadata {PATH}: {ERROR}",
                   "PATH", metadataPath, "ERROR", e);
    }
}

void Entry::deserialize(const std::filesystem::path& dumpPath)
{
    phosphor::dump::Entry::deserialize(dumpPath);

    const auto metadataPath = getMetadataPath(dumpPath);
    if (!std::filesystem::exists(metadataPath))
    {
        return;
    }

    try
    {
        std::ifstream stream(metadataPath, std::ios::binary);
        if (!stream.is_open())
        {
            lg2::error("Failed to open OpenPOWER metadata {PATH}", "PATH",
                       metadataPath);
            return;
        }

        nlohmann::json json;
        stream >> json;

        if (json.at("version").get<uint32_t>() != OP_SERIALIZATION_VERSION)
        {
            lg2::error("Unsupported OpenPOWER metadata version in {PATH}",
                       "PATH", metadataPath);
            return;
        }

        const auto storedId = json.at("dumpId").get<uint32_t>();
        if (storedId != id)
        {
            lg2::error(
                "OpenPOWER metadata ID {STORED_ID} does not match {DUMP_ID}",
                "STORED_ID", storedId, "DUMP_ID", id);
            return;
        }

        EntryMetadata metadata;
        if (json.contains("systemImpact"))
        {
            metadata.systemImpact = json.at("systemImpact").get<uint32_t>();
        }
        if (json.contains("userChallenge"))
        {
            metadata.userChallenge =
                json.at("userChallenge").get<std::string>();
        }
        if (json.contains("vspString"))
        {
            metadata.vspString = json.at("vspString").get<std::string>();
        }
        if (json.contains("errorLogId"))
        {
            metadata.errorLogId = json.at("errorLogId").get<uint64_t>();
        }
        if (json.contains("failingUnitId"))
        {
            metadata.failingUnitId = json.at("failingUnitId").get<uint64_t>();
        }

        restoreMetadata(metadata);
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to deserialize OpenPOWER metadata {PATH}: {ERROR}",
                   "PATH", metadataPath, "ERROR", e);
    }
}

} // namespace openpower::dump
