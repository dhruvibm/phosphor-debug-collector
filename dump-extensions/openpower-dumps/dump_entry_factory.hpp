#pragma once

#include "dump_entry.hpp"
#include "dump_manager.hpp"
#include "op_dump_util.hpp"

#include <sdbusplus/bus.hpp>

#include <filesystem>
#include <memory>
#include <string>

namespace openpower::dump
{

/**
 * @class DumpEntryFactory
 * @brief Factory class to create dump entries based on dump type.
 *        This class encapsulates the creation of dump entries.
 */
class DumpEntryFactory
{
  public:
    /**
     * @brief Constructs a dump entry factory.
     * @param[in] bus Reference to the D-Bus bus object.
     * @param[in] baseEntryPath Base object path for the dump entries.
     * @param[in] mgr Reference to the dump manager handling these dumps.
     */
    DumpEntryFactory(sdbusplus::bus_t& bus, const std::string& baseEntryPath,
                     phosphor::dump::Manager& mgr) :
        bus(bus), baseEntryPath(baseEntryPath), mgr(mgr)
    {}

    /**
     * @brief Creates a dump entry based on provided parameters.
     * @param[in] id The unique identifier for the new dump entry.
     * @param[in] params Parameters defining the dump creation specifics.
     * @return A unique pointer to a newly created dump entry, or nullptr if
     * creation fails.
     */
    std::unique_ptr<phosphor::dump::Entry> createEntry(
        uint32_t id, const phosphor::dump::DumpCreateParams& params);

    /**
     * @brief Creates a silently registered entry for restoration.
     * @param[in] id Persisted dump identifier containing the dump type.
     * @param[in] objPath D-Bus entry path for the restored entry.
     * @return The restored entry type, or nullptr for an unknown ID prefix.
     *
     * The returned entry does not emit InterfacesAdded. The manager restores
     * its properties before retaining the object.
     */
    std::unique_ptr<phosphor::dump::Entry> createEntryForRestore(
        uint32_t id, const std::filesystem::path& objPath);

  private:
    /**
     * @brief Creates a system dump entry.
     * @param[in] id The unique identifier for the system dump entry.
     * @param[in] objPath D-Bus entry path for the dump entry.
     * @param[in] timeStamp Timestamp marking the creation time of the dump.
     * @param[in] dumpParams Parameters specific to the dump being created.
     * @return A unique pointer to a newly created system dump entry.
     */
    std::unique_ptr<phosphor::dump::Entry> createSystemDumpEntry(
        uint32_t id, const std::filesystem::path& objPath, uint64_t timeStamp,
        const DumpParameters& dumpParams);

    /**
     * @brief Creates a resource dump entry.
     * @param[in] id The unique identifier for the resource dump entry.
     * @param[in] objPath D-Bus entry path for the dump entry.
     * @param[in] timeStamp Timestamp marking the creation time of the dump.
     * @param[in] dumpParams Parameters specific to the dump being created.
     * @return A unique pointer to a newly created resource dump entry.
     */
    std::unique_ptr<phosphor::dump::Entry> createResourceDumpEntry(
        uint32_t id, const std::filesystem::path& objPath, uint64_t timeStamp,
        const DumpParameters& dumpParams);

    /**
     * @brief Creates a Hostboot dump entry.
     * @param[in] id The unique identifier for the Hostboot dump entry.
     * @param[in] objPath D-Bus entry path for the dump entry.
     * @param[in] timeStamp Timestamp marking the creation time of the dump.
     * @param[in] dumpParams Parameters specific to the dump being created.
     * @return A unique pointer to a newly created Hostboot dump entry.
     */
    std::unique_ptr<phosphor::dump::Entry> createHostbootDumpEntry(
        uint32_t id, const std::filesystem::path& objPath, uint64_t timeStamp,
        const DumpParameters& dumpParams);

    /**
     * @brief Creates a Hardware dump entry.
     * @param[in] id The unique identifier for the Hardware dump entry.
     * @param[in] objPath D-Bus entry path for the dump entry.
     * @param[in] timeStamp Timestamp marking the creation time of the dump.
     * @param[in] dumpParams Parameters specific to the dump being created.
     * @return A unique pointer to a newly created Hardware dump entry.
     */
    std::unique_ptr<phosphor::dump::Entry> createHardwareDumpEntry(
        uint32_t id, const std::filesystem::path& objPath, uint64_t timeStamp,
        const DumpParameters& dumpParams);

    /**
     * @brief Creates an SBE dump entry.
     * @param[in] id The unique identifier for the SBE dump entry.
     * @param[in] objPath D-Bus entry path for the dump entry.
     * @param[in] timeStamp Timestamp marking the creation time of the dump.
     * @param[in] dumpParams Parameters specific to the dump being created.
     * @return A unique pointer to a newly created SBE dump entry.
     */
    std::unique_ptr<phosphor::dump::Entry> createSBEDumpEntry(
        uint32_t id, const std::filesystem::path& objPath, uint64_t timeStamp,
        const DumpParameters& dumpParams);

    /** @brief sdbusplus DBus bus connection. */
    sdbusplus::bus_t& bus;

    /** @brief Base D-Bus path for dump entries. */
    const std::string& baseEntryPath;

    /** @brief Reference to the managing object for dumps.*/
    phosphor::dump::Manager& mgr;
};

} // namespace openpower::dump
