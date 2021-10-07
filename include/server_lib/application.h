#pragma once

#include <server_lib/event_loop.h>

namespace server_lib {

/**
 * \ingroup common
 *
 * This is the multythread model class for application lifetime control,
 * careful signal processing and main thread management
*/
class application
{
protected:
    application();

    friend class __internal_application_manager;

public:
    /**
     * \ingroup common
     *
     * \brief This class wrap main thread to provide event_loop features.
     */
    class main_loop : public event_loop
    {
        friend class application_impl;

    protected:
        main_loop(const std::string& name = {});

        void start(std::function<void(void)> start_notify = nullptr,
                   std::function<void(void)> stop_notify = nullptr,
                   bool waiting_for_start = false) override;

    public:
        void stop() override;
    };

    class config
    {
        friend class application_impl;

    protected:
        config() = default;

    public:
        config(const config&) = default;
        ~config() = default;

        /**
         * \param path_to_dump_file - Full path to dump file to create.
         * Dump file contains brief crash info or in other words 'crush dump'.
         * Crush dump is only complementation for core dump (in core file)
         * but useful for monitoring
         *
         */
        config& enable_coredump(const std::string& path_to_dump_file);

        /**
         * \param disable_excl_policy - Allow multiple corefile creation
         *
         */
        config& enable_corefile(bool disable_excl_policy = true);

        /**
         * Add more facilities for fail_callback
         * but limit corefile with only one crushed thread and common memory
         *
         */
        config& corefile_fail_thread_only();

        /**
         * Ignore errors if try to write to STDOUT, STRERR.
         * Useful for silent applications daemonized by demon mode or external orchestrator
         *
         */
        config& lock_io();

        /**
         * Daemonize calling process
         * (As a rule it is not necessary because daemonizing belongs to orchestrator responsibilities)
         *
         */
        config& make_daemon();

    private:
        //default configuration
        std::string _path_to_dump_file;
        bool _enable_corefile = false;
        bool _corefile_fail_thread_only = false;
        bool _corefile_disable_excl_policy = false;
        bool _lock_io = false;
        bool _daemon = false;
    };

    enum class control_signal
    {
        USR1,
        USR2
    };

    using start_callback_type = std::function<void(void)>;
    using exit_callback_type = std::function<void(const int signo)>;
    using fail_callback_type = std::function<void(const int signo, const char* /*path_to_dump_file*/)>;
    using control_callback_type = std::function<void(const control_signal)>;

    /**
     * Configurate
     *
     */
    static config configurate();

    /**
     * Call before all thread created. It set application mode for daemonizing
     * and crash behavior
     *
     * \param config
     *
     * \return singleton of application object (to save memory and construction order requirements)
     *
     */
    static application& init(const config&);

    /**
     * Initialize with default configuration:
     * none coredump, system defined corefile settings,
     * unlocked SIGPIPE, none daemonizing
     *
     */
    static application& init();

    /**
     * \brief Start application and stuck in main loop
     *
     * Stuck into main thread untill apropriate callback will be called.
     * It invokes all callbacks (expect fail_callback) in main thread
     * where main_loop was runned.
     * It uses default exit and fail callbacks if those were not set.
     *
     * \return application exit code
     *
     */
    int run();

    /**
     * \brief Check if application running
     *
     * \return true - if application was run
     *
     */
    bool is_running();

    /**
     * \brief Invoke callback when application will start
     *
     * \param callback
     *
     * \return singleton of application object
     *
     */
    application& on_start(start_callback_type&& callback);

    /**
     * \brief Invoke callback when application exit by signal (SIGTERM, SIGINT)
     *  or 'exit' command
     *
     * \param callback
     *
     * \return singleton of application object
     *
     */
    application& on_exit(exit_callback_type&& callback);

    /**
     * \brief Invoke callback when application fail by signal.
     * Callback will be invoked in thread that was fail source.
     *
     * \param callback
     *
     * \return singleton of application object
     *
     */
    application& on_fail(fail_callback_type&& callback);

    /**
     * \brief Invoke callback when application receive control signal (SIGUSR1, SIGUSR2)
     *
     * \param callback
     *
     * \return singleton of application object
     *
     */
    application& on_control(control_callback_type&& callback);

    /**
     * \brief Waiting for finish for starting procedure
     * or immediately (if application has already started)
     * return
     *
     */
    void wait();

    /**
     * Return reference to main loop used by this application.
     * This loop wraps main thread and receive all registered callbacks
     *
     * \return reference to main loop
     *
     */
    main_loop& loop();

    /**
     * \brief Stop application and exit with code if was set
     *
     * \param exit_code
     *
     * This makes exit from 'run'
     *
     */
    void stop(int exit_code = 0);
};

using application_main_loop = application::main_loop;
using application_config = application::config;

} // namespace server_lib
