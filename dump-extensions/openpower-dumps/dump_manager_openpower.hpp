#pragma once

#include "dump_manager.hpp"

#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Dump/Create/server.hpp>

namespace openpower::dump
{

using OpDumpIfaces = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Dump::server::Create>;

/** @class Manager
 *  @brief OpenPOWER dump manager implementation.
 *  @details A concrete implementation for the
 *  xyz.openbmc_project.Dump.Create D-Bus API.
 */
class Manager :
    virtual public OpDumpIfaces,
    virtual public phosphor::dump::Manager
{
  public:
    Manager() = delete;
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    Manager(Manager&&) = delete;
    Manager& operator=(Manager&&) = delete;
    virtual ~Manager() = default;

    /** @brief Constructor to put object onto bus at a dbus path.
     *  @param[in] bus - Bus to attach to.
     *  @param[in] event - Dump manager sd_event loop.
     *  @param[in] path - Path to attach at.
     *  @param[in] baseEntryPath - Base path of the dump entry.
     */
    Manager(sdbusplus::bus_t& bus, const char* path,
            const std::string& baseEntryPath) :
        OpDumpIfaces(bus, path),
        phosphor::dump::Manager(bus, path, baseEntryPath)
    {}

    void restore() override
    {
        // TODO #2597  Implement the restore to restore the dump entries
        // after the service restart.
    }

    /** @brief Implementation for CreateDump
     *  Method to create a new OpenPOWER dump entry.
     *
     *  @return object_path - The path to the new dump entry.
     */
    sdbusplus::object_path createDump(
        phosphor::dump::DumpCreateParams params) override;
};

} // namespace openpower::dump
