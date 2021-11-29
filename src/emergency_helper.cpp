#include <server_lib/emergency_helper.h>
#include <server_lib/platform_config.h>
#include <server_lib/asserts.h>

#include <boost/version.hpp>
#include <boost/filesystem.hpp>
#include <boost/stacktrace.hpp>

#include <iostream>
#include <sstream>
#include <fstream>

namespace {

static char MAGIC_FOR_DUMP_FMT[] = "~stack trace:\n\n";

} // namespace

namespace server_lib {
namespace emergency {

    bool test_file_for_write(const char* file_path)
    {
        if (boost::filesystem::exists(file_path))
            return false;

        {
            std::ofstream ofs(file_path);

            if (!ofs)
                return false;
        }

        boost::filesystem::remove(file_path);
        return true;
    }

    bool save_raw_stdump_s(const char* raw_dump_file_path)
    {
        // Dumps are binary serialized arrays of void*, so you could read them by using
        // 'od -tx8 -An stacktrace_dump_failename' Linux command
        // or using boost::stacktrace::stacktrace::from_dump functions.
        return boost::stacktrace::safe_dump_to(raw_dump_file_path) > 0;
    }

    std::string load_raw_stdump(const char* raw_dump_file_path, bool remove)
    {
        std::stringstream ss;

        if (boost::filesystem::exists(raw_dump_file_path))
        {
            try
            {
                std::ifstream ifs(raw_dump_file_path);

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
                    boost::filesystem::remove(raw_dump_file_path);
            }
            catch (const std::exception&)
            {
                // ignore
            }
        }

        return ss.str();
    }

    bool save_demangled_stdump(const char* raw_dump_file_path, const char* demangled_file_path)
    {
        if (!boost::filesystem::exists(raw_dump_file_path))
            return false;

        auto demangled = load_raw_stdump(raw_dump_file_path, false);
        if (demangled.empty())
            return false;

        std::ofstream out(demangled_file_path, std::ifstream::binary);

        if (!out)
            return false;

        out.write(MAGIC_FOR_DUMP_FMT, sizeof(MAGIC_FOR_DUMP_FMT));
        out.write(demangled.c_str(), demangled.size());

        return true;
    }

    std::string load_stdump(const char* dump_file_path, bool remove)
    {
        std::ifstream input(dump_file_path, std::ifstream::binary);

        if (!input)
            return {};

        char buff[sizeof(MAGIC_FOR_DUMP_FMT)];
        if (!input.read(buff, sizeof(buff)))
            return {};

        std::string dump;

        std::streamsize bytes_read = input.gcount();
        if (bytes_read == sizeof(MAGIC_FOR_DUMP_FMT) && 0 == memcmp(buff, MAGIC_FOR_DUMP_FMT, bytes_read))
        {
            std::stringstream ss;

            for (; input.read(buff, sizeof(buff)) || bytes_read > 0;)
            {
                bytes_read = input.gcount();
                if (bytes_read > 0)
                {
                    ss.write(buff, static_cast<uint32_t>(bytes_read));
                }
            }

            input.close();

            dump = ss.str();
        }
        else
        {
            input.close();

            dump = load_raw_stdump(dump_file_path, false);
        }

        try
        {
            if (remove)
                boost::filesystem::remove(dump_file_path);
        }
        catch (const std::exception&)
        {
            // ignore
        }

        return dump;
    }

    std::string get_executable_name()
    {
        std::string name;
#if defined(SERVER_LIB_PLATFORM_LINUX)

        std::ifstream("/proc/self/comm") >> name;

#elif defined(SERVER_LIB_PLATFORM_WINDOWS)

        char buf[MAX_PATH];
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        name = buf;

#endif
        return name;
    }

    std::string create_dump_filename(const std::string& extension)
    {
        SRV_ASSERT(!extension.empty(), "Extension required");

        std::string name = get_executable_name();
        if (name.empty())
            name = "core";

        name.push_back('.');
        name.append(extension);

        return name;
    }

} // namespace emergency
} // namespace server_lib
