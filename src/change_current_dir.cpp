#include <server_lib/change_current_dir.h>
#include <server_lib/asserts.h>

#include <boost/filesystem.hpp>

#include "logger_set_internal_group.h"

namespace server_lib {

namespace bfs = boost::filesystem;

change_current_dir::change_current_dir(const boost::filesystem::path& dir)
{
    prev_dir = bfs::current_path();
    SRV_ASSERT(bfs::exists(dir) && bfs::is_directory(dir));
    try
    {
        bfs::current_path(dir);
    }
    catch (const std::exception& e)
    {
        SRV_THROW();
    }
}

change_current_dir::~change_current_dir()
{
    try
    {
        bfs::current_path(prev_dir);
    }
    catch (const std::exception& e)
    {
        SRV_THROW();
    }
}

} // namespace server_lib
