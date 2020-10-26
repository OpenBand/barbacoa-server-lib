#pragma once

#include <stdexcept>

namespace server_lib {

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
#ifdef _DEBUG
        if (p_instance == NULL)
            throw std::logic_error("singleton get_instance failed");
#endif
        return *p_instance;
    }

    /**
     * Destroy current instance
     */
    static void destroy()
    {
        if (p_instance() != NULL)
        {
            delete p_instance();
            (*pp_instance()) = NULL;
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
        static Singleton* p_instance = NULL; // single object instance
        return &p_instance;
    }
};
} // namespace server_lib
