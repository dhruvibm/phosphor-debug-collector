#pragma once

#include "dump_utils.hpp"
#include "op_dump_policy.hpp"

#include <com/ibm/Dump/Create/common.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace openpower::dump
{
/**
 * @struct DumpParameters
 * @brief Holds parameters relevant to dump creation.
 *
 * This structure encapsulates all necessary parameters for creating a dump,
 * including optional and mandatory fields based on the type of dump.
 */
struct DumpParameters
{
    using SBEDumpTriggerType =
        sdbusplus::common::com::ibm::dump::Create::SBEDumpTriggerType;

    OpDumpTypes type;
    std::optional<std::string> vspString;
    std::optional<std::string> userChallenge;
    std::optional<std::string> acfPath;
    std::optional<uint64_t> eid;
    std::optional<uint64_t> fid;
    std::string originatorId;
    phosphor::dump::originatorTypes originatorType;
    std::optional<std::string> dumpFilesPath;
    std::optional<SBEDumpTriggerType> sbeDumpTriggerType;
};

namespace util
{

/** @brief Check whether automatic OpenPOWER dumps are enabled
 *
 * @param[in] bus - D-Bus handle
 *
 * If the settings service is not running, dumps are considered enabled.
 * @return true - if dumps are enabled, false - if dumps are not enabled
 */
bool isOPDumpsEnabled(sdbusplus::bus_t& bus);

using BIOSAttrValueType = std::variant<int64_t, std::string>;

/** @brief Read a BIOS attribute value
 *
 *  @param[in] attrName - Name of the BIOS attribute
 *  @param[in] bus - D-Bus handle
 *
 *  @return The value of the BIOS attribute as a variant of possible types
 *
 *  @throws sdbusplus::exception::SdBusError if failed to read the attribute
 */
BIOSAttrValueType readBIOSAttribute(const std::string& attrName,
                                    sdbusplus::bus_t& bus);

/** @brief Check whether a system is in progress or available to offload.
 *
 *  @param[in] bus - D-Bus handle
 *
 *  @return true - A dump is in progress or available to offload
 *          false - No dump in progress
 */
bool isSystemDumpInProgress(sdbusplus::bus_t& bus);

/**
 * @brief Extracts and constructs a DumpParameters structure from a set of
 * parameters.
 *
 * @param[in] params The map containing the parameters.
 * @return A constructed DumpParameters structure.
 */
openpower::dump::DumpParameters extractDumpParameters(
    const phosphor::dump::DumpCreateParams& params);

/**
 * @brief Throws a standardized invalid argument error.
 *
 * @param[in] argumentName The name of the argument that is invalid.
 * @param[in] errorDetail The value or reason why the argument is considered
 * invalid.
 */
[[noreturn]] void throwInvalidArgument(const std::string& argumentName,
                                       const std::string& errorDetail);

/** @brief Convert a YYYYMMDDhhmmss timestamp to epoch microseconds. */
uint64_t timeToEpoch(const std::string& timeString);
} // namespace util
} // namespace openpower::dump
