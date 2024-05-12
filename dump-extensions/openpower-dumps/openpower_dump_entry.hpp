#pragma once

#include "dump_entry.hpp"
#include "op_dump_consts.hpp"

#include <com/ibm/Dump/Entry/Hardware/server.hpp>
#include <com/ibm/Dump/Entry/Hostboot/server.hpp>
#include <com/ibm/Dump/Entry/Resource/server.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Dump/Entry/System/server.hpp>

#include <filesystem>
#include <string>
#include <utility>

namespace openpower::dump
{

using originatorTypes = sdbusplus::xyz::openbmc_project::Common::server::
    OriginatedBy::OriginatorTypes;

/** @class Entry
 *  @brief File-backed base entry for OpenPOWER dumps.
 */
class Entry : public virtual phosphor::dump::Entry
{
  public:
    Entry() = delete;
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    Entry(Entry&&) = delete;
    Entry& operator=(Entry&&) = delete;
    virtual ~Entry() = default;

    /** @brief Construct an OpenPOWER dump entry.
     *  @param[in] bus D-Bus connection.
     *  @param[in] objPath D-Bus object path.
     *  @param[in] dumpId Unique dump identifier.
     *  @param[in] timeStamp Dump creation timestamp.
     *  @param[in] fileSize Dump file size.
     *  @param[in] file Dump file path.
     *  @param[in] status Dump operation status.
     *  @param[in] originatorId Dump originator identifier.
     *  @param[in] originatorType Dump originator type.
     *  @param[in] parent Parent dump manager.
     */
    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          uint64_t timeStamp, uint64_t fileSize,
          const std::filesystem::path& file,
          phosphor::dump::OperationStatus status, std::string originatorId,
          originatorTypes originatorType, phosphor::dump::Manager& parent) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, timeStamp, fileSize,
                              file, status, originatorId, originatorType,
                              parent)
    {}

    /** @brief Delete the local dump and its D-Bus entry. */
    void delete_() override;

    /** @brief Offload the local dump file. */
    void initiateOffload(std::string uri) override;

    /** @brief Complete an entry after its dump file is written locally.
     *  @param[in] timeStamp Dump completion timestamp.
     *  @param[in] fileSize Dump file size.
     *  @param[in] filePath Dump file path.
     */
    void update(uint64_t timeStamp, uint64_t fileSize,
                const std::filesystem::path& filePath)
    {
        file = filePath;
        elapsed(timeStamp);
        size(fileSize);
        status(OperationStatus::Completed);
        completedTime(timeStamp);
    }
};

namespace system
{

using SystemIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Dump::Entry::server::System>;
using SystemImpact =
    sdbusplus::common::xyz::openbmc_project::dump::entry::System::SystemImpact;

/** @class Entry
 *  @brief File-backed System dump entry.
 */
class Entry : public virtual openpower::dump::Entry, public virtual SystemIntf
{
  public:
    Entry() = delete;
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    Entry(Entry&&) = delete;
    Entry& operator=(Entry&&) = delete;
    ~Entry() = default;

    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          uint64_t timeStamp, uint64_t fileSize,
          phosphor::dump::OperationStatus status, std::string originatorId,
          originatorTypes originatorType, phosphor::dump::Manager& parent) :
        Entry(bus, objPath, dumpId, timeStamp, fileSize, status, originatorId,
              originatorType, SystemImpact::Disruptive, std::string(), parent)
    {}

    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          uint64_t timeStamp, uint64_t fileSize,
          phosphor::dump::OperationStatus status, std::string originatorId,
          originatorTypes originatorType, SystemImpact impact,
          std::string userChallengeValue, phosphor::dump::Manager& parent) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, timeStamp, fileSize,
                              std::filesystem::path(), status, originatorId,
                              originatorType, parent),
        openpower::dump::Entry(bus, objPath, dumpId, timeStamp, fileSize,
                               std::filesystem::path(), status, originatorId,
                               originatorType, parent),
        SystemIntf(bus, objPath.c_str(), SystemIntf::action::defer_emit)
    {
        sourceDumpId(INVALID_SOURCE_ID);
        systemImpact(impact);
        userChallenge(std::move(userChallengeValue));
        this->SystemIntf::emit_object_added();
    }
};

} // namespace system

namespace resource
{

using ResourceIntf = sdbusplus::server::object_t<
    sdbusplus::com::ibm::Dump::Entry::server::Resource>;

/** @class Entry
 *  @brief File-backed Resource dump entry.
 */
class Entry : public virtual openpower::dump::Entry, public virtual ResourceIntf
{
  public:
    Entry() = delete;
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    Entry(Entry&&) = delete;
    Entry& operator=(Entry&&) = delete;
    ~Entry() = default;

    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          uint64_t timeStamp, uint64_t fileSize, std::string vspSelector,
          std::string userChallengeValue,
          phosphor::dump::OperationStatus status, std::string originatorId,
          originatorTypes originatorType, phosphor::dump::Manager& parent) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, timeStamp, fileSize,
                              std::filesystem::path(), status, originatorId,
                              originatorType, parent),
        openpower::dump::Entry(bus, objPath, dumpId, timeStamp, fileSize,
                               std::filesystem::path(), status, originatorId,
                               originatorType, parent),
        ResourceIntf(bus, objPath.c_str(), ResourceIntf::action::defer_emit)
    {
        sourceDumpId(INVALID_SOURCE_ID);
        vspString(std::move(vspSelector));
        userChallenge(std::move(userChallengeValue));
        this->ResourceIntf::emit_object_added();
    }
};

} // namespace resource

namespace hostboot
{

using HostbootIntf = sdbusplus::server::object_t<
    sdbusplus::com::ibm::Dump::Entry::server::Hostboot>;

/** @class Entry
 *  @brief File-backed Hostboot dump entry.
 */
class Entry : public virtual openpower::dump::Entry, public virtual HostbootIntf
{
  public:
    Entry() = delete;
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    Entry(Entry&&) = delete;
    Entry& operator=(Entry&&) = delete;
    ~Entry() = default;

    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          uint64_t timeStamp, uint64_t fileSize,
          const std::filesystem::path& file,
          phosphor::dump::OperationStatus status, std::string originatorId,
          originatorTypes originatorType, uint64_t errorLogIdValue,
          phosphor::dump::Manager& parent) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, timeStamp, fileSize,
                              file, status, originatorId, originatorType,
                              parent),
        openpower::dump::Entry(bus, objPath, dumpId, timeStamp, fileSize, file,
                               status, originatorId, originatorType, parent),
        HostbootIntf(bus, objPath.c_str(), HostbootIntf::action::defer_emit)
    {
        errorLogId(errorLogIdValue);
        this->HostbootIntf::emit_object_added();
    }
};

} // namespace hostboot

namespace hardware
{

using HardwareIntf = sdbusplus::server::object_t<
    sdbusplus::com::ibm::Dump::Entry::server::Hardware>;

/** @class Entry
 *  @brief File-backed Hardware dump entry.
 */
class Entry : public virtual openpower::dump::Entry, public virtual HardwareIntf
{
  public:
    Entry() = delete;
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    Entry(Entry&&) = delete;
    Entry& operator=(Entry&&) = delete;
    ~Entry() = default;

    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          uint64_t timeStamp, uint64_t fileSize,
          const std::filesystem::path& file,
          phosphor::dump::OperationStatus status, std::string originatorId,
          originatorTypes originatorType, uint64_t errorLogIdValue,
          uint64_t failingUnitIdValue, phosphor::dump::Manager& parent) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, timeStamp, fileSize,
                              file, status, originatorId, originatorType,
                              parent),
        openpower::dump::Entry(bus, objPath, dumpId, timeStamp, fileSize, file,
                               status, originatorId, originatorType, parent),
        HardwareIntf(bus, objPath.c_str(), HardwareIntf::action::defer_emit)
    {
        errorLogId(errorLogIdValue);
        failingUnitId(failingUnitIdValue);
        this->HardwareIntf::emit_object_added();
    }
};

} // namespace hardware

} // namespace openpower::dump
