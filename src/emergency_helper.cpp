#include <server_lib/emergency_helper.h>

#if !defined(STACKTRACE_DISABLED)

#include <boost/version.hpp>
#include <boost/filesystem.hpp>

#include <iostream>
#include <sstream>
#include <fstream>

#if !defined(CUSTOM_STACKTRACE_IMPL) && BOOST_VERSION >= 106500

#include <boost/stacktrace.hpp>

namespace server_lib {

void emergency_helper::save_dump(const char* dump_file_path)
{
    try
    {
        boost::stacktrace::safe_dump_to(dump_file_path);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Can't save dump: " << e.what() << std::endl;
    }
}

std::string emergency_helper::load_dump(const char* dump_file_path, bool remove)
{
    std::stringstream ss;

    if (boost::filesystem::exists(dump_file_path))
    {
        try
        {
            std::ifstream ifs(dump_file_path);

            boost::stacktrace::stacktrace st = boost::stacktrace::stacktrace::from_dump(ifs);
            ss << st;

            // cleaning up
            ifs.close();
        }
        catch (const std::exception& e)
        {
            ss << e.what();
        }
        ss << '\n';

        try
        {
            if (remove)
                boost::filesystem::remove(dump_file_path);
        }
        catch (const std::exception&)
        {
            // ignore
        }
    }

    return ss.str();
}
} // namespace server_lib

#else

#include <stdlib.h>
#include <execinfo.h>
#include <cxxabi.h>

namespace server_lib {

void emergency_helper::save_dump(const char* dump_file_path)
{
    FILE* out = fopen(dump_file_path, "w");

    if (!out)
    {
        perror("Can't save dump");
        return;
    }

    /** Print a demangled stack backtrace of the caller function to FILE* out. */

    const size_t max_frames = 100;

    fprintf(out, "stack trace:\n");

    // storage array for stack trace address data
    void* addrlist[max_frames + 1] = {};

    // retrieve current stack addresses:
    //                                                  https://linux.die.net/man/3/backtrace_symbols:
    //"The symbol names may be unavailable without the use of special linker options.
    // For systems using the GNU linker, it is necessary to use the -rdynamic linker option.
    // Note that names of "static" functions are not exposed, and won't be available in the backtrace."
    //
    int addrlen = backtrace(addrlist, sizeof(addrlist) / sizeof(void*));

    if (addrlen == 0)
    {
        fprintf(out, "  <empty, possibly corrupt>\n");
        return;
    }

    // resolve addresses into strings containing "filename(function+address)",
    // this array must be free()-ed
    char** symbollist = backtrace_symbols(addrlist, addrlen);

    // allocate string which will be filled with the demangled function name
    size_t funcnamesize = 256;
    char* funcname = reinterpret_cast<char*>(alloca(funcnamesize));

    // iterate over the returned symbol lines. skip the first, it is the
    // address of this function.
    for (int i = 1; i < addrlen; i++)
    {
        char *begin_name = nullptr, *begin_offset = nullptr, *end_offset = nullptr;

        // find parentheses and +address offset surrounding the mangled name:
        // ./module(function+0x15c) [0x8048a6d]
        for (char* p = symbollist[i]; *p; ++p)
        {
            if (*p == '(')
                begin_name = p;
            else if (*p == '+')
                begin_offset = p;
            else if (*p == ')' && begin_offset)
            {
                end_offset = p;
                break;
            }
        }

        if (begin_name && begin_offset && end_offset
            && begin_name < begin_offset)
        {
            *begin_name++ = '\0';
            *begin_offset++ = '\0';
            *end_offset = '\0';

            // mangled name is now in [begin_name, begin_offset) and caller
            // offset in [begin_offset, end_offset). now apply
            // __cxa_demangle():

            int status;
            char* ret = abi::__cxa_demangle(begin_name,
                                            funcname, &funcnamesize, &status);
            if (status == 0)
            {
                funcname = ret; // use possibly realloc()-ed string
                fprintf(out, "  %s : %s+%s\n",
                        symbollist[i], funcname, begin_offset);
            }
            else
            {
                // demangling failed. Output function name as a C function with
                // no arguments.
                fprintf(out, "  %s : %s()+%s\n",
                        symbollist[i], begin_name, begin_offset);
            }
        }
        else
        {
            // couldn't parse the line? print the whole line.
            fprintf(out, "  %s\n", symbollist[i]);
        }
    }

    fclose(out);

    free(symbollist);
}

std::string emergency_helper::load_dump(const char* dump_file_path, bool remove)
{
    if (dump_file_path && boost::filesystem::exists(dump_file_path))
    {
        std::stringstream ss;

        try
        {
            std::ifstream ifs(dump_file_path);

            std::string line;
            while (std::getline(ifs, line))
            {
                ss << line << '\n';
            }

            // cleaning up
            ifs.close();
        }
        catch (const std::exception& e)
        {
            ss << e.what();
        }
        ss << '\n';

        try
        {
            if (remove)
                boost::filesystem::remove(dump_file_path);
        }
        catch (const std::exception&)
        {
            // ignore
        }

        return ss.str();
    }
    else
        return {};
}
} // namespace server_lib
#endif
namespace server_lib {
bool emergency_helper::test_for_write(const char* dump_file_path)
{
    if (boost::filesystem::exists(dump_file_path))
        return false;

    {
        std::ofstream ofs(dump_file_path);

        if (!ofs)
            return false;
    }

    boost::filesystem::remove(dump_file_path);
    return true;
}
} // namespace server_lib
#else //!STACKTRACE_DISABLED

// raise linker error for save_dump, load_dump
#endif
