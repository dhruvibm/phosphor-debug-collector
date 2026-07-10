#pragma once

#include "dump_entry.hpp"
#include "op_dump_consts.hpp"

#include <com/ibm/Dump/Create/common.hpp>
#include <com/ibm/Dump/Entry/Hardware/server.hpp>
#include <com/ibm/Dump/Entry/Hostboot/server.hpp>
#include <com/ibm/Dump/Entry/Resource/server.hpp>
#include <com/ibm/Dump/Entry/SBE/server.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Dump/Entry/System/server.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <utility>

namespace openpower::dump
{

using originatorTypes = sdbusplus::xyz::openbmc_project::Common::server::
    OriginatedBy::OriginatorTypes;

/** @brief OpenPOWER properties not covered by the common serialization. */
struct EntryMetadata
{
    std::optional<uint32_t> systemImpact;
    std::optional<std::string> userChallenge;
    std::optional<std::string> vspString;
    std::optional<uint64_t> errorLogId;
    std::optional<uint64_t> failingUnitId;
};

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

    /** @brief Construct a silently registered entry for restoration. */
    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          phosphor::dump::Manager& parent) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, 0, 0,
                              std::filesystem::path(),
                              phosphor::dump::OperationStatus::InProgress,
                              std::string(), originatorTypes::Internal, parent)
    {}

    /** @brief Delete the local dump and its D-Bus entry. */
    void delete_() override;

    /** @brief Offload the local dump file. */
    void initiateOffload(std::string uri) override;

    /** @brief Serialize common and OpenPOWER-specific properties. */
    void serialize() override;

    /** @brief Restore common and OpenPOWER-specific properties. */
    void deserialize(const std::filesystem::path& dumpPath) override;

    /** @brief Complete an entry after its dump file is written locally.
     *  @param[in] timeStamp Dump completion timestamp.
     *  @param[in] fileSize Dump file size.
     *  @param[in] filePath Dump file path.
     */
    void update(uint64_t timeStamp, uint64_t fileSize,
                const std::filesystem::path& filePath)
    {
        complete(timeStamp, fileSize, filePath);
        serialize();
    }

    /** @brief Restore completed file properties without reserializing. */
    void restoreFile(uint64_t timeStamp, uint64_t fileSize,
                     const std::filesystem::path& filePath)
    {
        complete(timeStamp, fileSize, filePath);
    }

  protected:
    /** @brief Return properties owned by the derived dump interface. */
    virtual EntryMetadata getMetadata() const
    {
        return {};
    }

    /** @brief Apply properties owned by the derived dump interface. */
    virtual void restoreMetadata(const EntryMetadata& /*metadata*/) {}

  private:
    void complete(uint64_t timeStamp, uint64_t fileSize,
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

    /** @brief Construct a restored System entry without InterfacesAdded. */
    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          phosphor::dump::Manager& parent) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, 0, 0,
                              std::filesystem::path(),
                              phosphor::dump::OperationStatus::InProgress,
                              std::string(), originatorTypes::Internal, parent),
        openpower::dump::Entry(bus, objPath, dumpId, parent),
        SystemIntf(bus, objPath.c_str(), SystemIntf::action::defer_emit)
    {
        sourceDumpId(INVALID_SOURCE_ID);
        systemImpact(SystemImpact::Disruptive);
    }

  protected:
    EntryMetadata getMetadata() const override
    {
        EntryMetadata metadata;
        metadata.systemImpact = static_cast<uint32_t>(systemImpact());
        metadata.userChallenge = userChallenge();
        return metadata;
    }

    void restoreMetadata(const EntryMetadata& metadata) override
    {
        if (metadata.systemImpact.has_value())
        {
            systemImpact(
                static_cast<SystemImpact>(metadata.systemImpact.value()));
        }
        if (metadata.userChallenge.has_value())
        {
            userChallenge(metadata.userChallenge.value());
        }
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

    /** @brief Construct a restored Resource entry without InterfacesAdded. */
    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          phosphor::dump::Manager& parent) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, 0, 0,
                              std::filesystem::path(),
                              phosphor::dump::OperationStatus::InProgress,
                              std::string(), originatorTypes::Internal, parent),
        openpower::dump::Entry(bus, objPath, dumpId, parent),
        ResourceIntf(bus, objPath.c_str(), ResourceIntf::action::defer_emit)
    {
        sourceDumpId(INVALID_SOURCE_ID);
    }

  protected:
    EntryMetadata getMetadata() const override
    {
        EntryMetadata metadata;
        metadata.userChallenge = userChallenge();
        metadata.vspString = vspString();
        return metadata;
    }

    void restoreMetadata(const EntryMetadata& metadata) override
    {
        if (metadata.userChallenge.has_value())
        {
            userChallenge(metadata.userChallenge.value());
        }
        if (metadata.vspString.has_value())
        {
            vspString(metadata.vspString.value());
        }
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

    /** @brief Construct a restored Hostboot entry without InterfacesAdded. */
    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          phosphor::dump::Manager& parent) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, 0, 0,
                              std::filesystem::path(),
                              phosphor::dump::OperationStatus::InProgress,
                              std::string(), originatorTypes::Internal, parent),
        openpower::dump::Entry(bus, objPath, dumpId, parent),
        HostbootIntf(bus, objPath.c_str(), HostbootIntf::action::defer_emit)
    {}

  protected:
    EntryMetadata getMetadata() const override
    {
        EntryMetadata metadata;
        metadata.errorLogId = errorLogId();
        return metadata;
    }

    void restoreMetadata(const EntryMetadata& metadata) override
    {
        if (metadata.errorLogId.has_value())
        {
            errorLogId(metadata.errorLogId.value());
        }
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

    /** @brief Construct a restored Hardware entry without InterfacesAdded. */
    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          phosphor::dump::Manager& parent) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, 0, 0,
                              std::filesystem::path(),
                              phosphor::dump::OperationStatus::InProgress,
                              std::string(), originatorTypes::Internal, parent),
        openpower::dump::Entry(bus, objPath, dumpId, parent),
        HardwareIntf(bus, objPath.c_str(), HardwareIntf::action::defer_emit)
    {}

  protected:
    EntryMetadata getMetadata() const override
    {
        EntryMetadata metadata;
        metadata.errorLogId = errorLogId();
        metadata.failingUnitId = failingUnitId();
        return metadata;
    }

    void restoreMetadata(const EntryMetadata& metadata) override
    {
        if (metadata.errorLogId.has_value())
        {
            errorLogId(metadata.errorLogId.value());
        }
        if (metadata.failingUnitId.has_value())
        {
            failingUnitId(metadata.failingUnitId.value());
        }
    }
};

} // namespace hardware

namespace sbe
{

using SBEIntf =
    sdbusplus::server::object_t<sdbusplus::com::ibm::Dump::Entry::server::SBE>;
using SBEDumpTriggerType =
    sdbusplus::common::com::ibm::dump::Create::SBEDumpTriggerType;

/** @class Entry
 *  @brief File-backed SBE dump entry.
 */
class Entry : public virtual openpower::dump::Entry, public virtual SBEIntf
{
  public:
    Entry() = delete;
    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;
    Entry(Entry&&) = delete;
    Entry& operator=(Entry&&) = delete;
    ~Entry() = default;

    /** @brief Constructor for the SBE Dump Entry Object
     *  @param[in] bus - Bus to attach to.
     *  @param[in] objPath - Object path to attach to.
     *  @param[in] dumpId - Unique identifier for the dump.
     *  @param[in] timeStamp - Dump creation timestamp since the epoch.
     *  @param[in] fileSize - Size of the dump file in bytes.
     *  @param[in] file - Path to the dump file.
     *  @param[in] status - Current status of the dump.
     *  @param[in] originatorId - Identifier of the originator of the dump.
     *  @param[in] originatorType - Type of the originator.
     *  @param[in] errorLogIdValue - Associated error log identifier.
     *  @param[in] failingUnitIdValue - Identifier of the failing unit.
     *  @param[in] parent - Reference to the managing dump manager.
     *  @param[in] dumpFilesPathValue - Optional caller-provided files path.
     *  @param[in] triggerType - Optional SBE dump trigger type.
     */
    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          uint64_t timeStamp, uint64_t fileSize,
          const std::filesystem::path& file,
          phosphor::dump::OperationStatus status, std::string originatorId,
          originatorTypes originatorType, uint64_t errorLogIdValue,
          uint64_t failingUnitIdValue, phosphor::dump::Manager& parent,
          const std::optional<std::string>& dumpFilesPathValue,
          const std::optional<SBEDumpTriggerType>& triggerType) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, timeStamp, fileSize,
                              file, status, originatorId, originatorType,
                              parent),
        openpower::dump::Entry(bus, objPath, dumpId, timeStamp, fileSize, file,
                               status, originatorId, originatorType, parent),
        SBEIntf(bus, objPath.c_str(), SBEIntf::action::defer_emit)
    {
        errorLogId(errorLogIdValue);
        failingUnitId(failingUnitIdValue);
        if (dumpFilesPathValue.has_value())
        {
            dumpFilesPath(*dumpFilesPathValue);
        }
        if (triggerType.has_value())
        {
            sbeDumpTriggerType(*triggerType);
        }

        this->SBEIntf::emit_object_added();
    }

    /** @brief Construct a restored SBE entry without InterfacesAdded. */
    Entry(sdbusplus::bus_t& bus, const std::string& objPath, uint32_t dumpId,
          phosphor::dump::Manager& parent) :
        phosphor::dump::Entry(bus, objPath.c_str(), dumpId, 0, 0,
                              std::filesystem::path(),
                              phosphor::dump::OperationStatus::InProgress,
                              std::string(), originatorTypes::Internal, parent),
        openpower::dump::Entry(bus, objPath, dumpId, parent),
        SBEIntf(bus, objPath.c_str(), SBEIntf::action::defer_emit)
    {}

  protected:
    EntryMetadata getMetadata() const override
    {
        EntryMetadata metadata;
        metadata.errorLogId = errorLogId();
        metadata.failingUnitId = failingUnitId();
        return metadata;
    }

    void restoreMetadata(const EntryMetadata& metadata) override
    {
        if (metadata.errorLogId.has_value())
        {
            errorLogId(metadata.errorLogId.value());
        }
        if (metadata.failingUnitId.has_value())
        {
            failingUnitId(metadata.failingUnitId.value());
        }
    }
};

} // namespace sbe

} // namespace openpower::dump
