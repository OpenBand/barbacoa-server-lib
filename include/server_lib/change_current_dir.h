#pragma once

#include <boost/filesystem/path.hpp>

namespace server_lib {

/**
 * \ingroup common
 *
 * \brief This class changes current directory in visibility scope
 */
class change_current_dir
{
public:
    using path = boost::filesystem::path;

    change_current_dir(const path& dir);
    ~change_current_dir();

private:
    path prev_dir;
};

} // namespace server_lib
