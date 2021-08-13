#pragma once

namespace server_lib {

/**
 * \ingroup common
 *
 * \brief Design patterns: Singleton
 */
template <class Singleton>
class singleton
{
protected:
    singleton()
    {
    }
    ~singleton()
    {
    }

public:
    /**
     * Returns existing instance or creats a new
     */
    static Singleton* get_instance()
    {
        if (!p_instance())
            (*pp_instance()) = new Singleton();
        return p_instance();
    }

    static Singleton& instance()
    {
        Singleton* p_instance = get_instance();
        return *p_instance;
    }

    /**
     * Destroy current instance
     */
    static void destroy()
    {
        if (p_instance() != nullptr)
        {
            delete p_instance();
            (*pp_instance()) = nullptr;
        }
    }

protected:
    static Singleton* p_instance()
    {
        return *pp_instance();
    }

private:
    static Singleton** pp_instance()
    {
        static Singleton* p_instance = nullptr; // single object instance
        return &p_instance;
    }
};
} // namespace server_lib
