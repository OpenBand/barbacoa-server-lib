#include <server_lib/change_current_dir.h>
#include <server_lib/asserts.h>

#include <boost/filesystem.hpp>

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
        SRV_ERROR(e.what());
    }
}

change_current_dir::~change_current_dir()
{
    try
    {
        bfs::current_path(prev_dir);
    }
    catch (...)
    {
    }
}

} // namespace server_lib
