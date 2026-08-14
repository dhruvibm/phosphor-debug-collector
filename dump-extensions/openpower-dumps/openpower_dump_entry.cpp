#include "openpower_dump_entry.hpp"

#include "dump_manager.hpp"
#include "dump_offload.hpp"

#include <phosphor-logging/lg2.hpp>

namespace openpower::dump
{

void Entry::delete_()
{
    if (!file.empty())
    {
        try
        {
            std::filesystem::remove(file);
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

} // namespace openpower::dump
