#include "err_emulator.h"

#include <server_lib/asserts.h>
#include <server_clib/macro.h>

#include <iostream>
#include <sstream>

namespace server_lib {

void try_libassert(uint n)
{
    std::cerr << __FUNCTION__ << std::endl;
    for (;;)
    {
        SRV_ASSERT(n > n * 2, "Assert for invalid condition");
    }
}

void try_clibassert(uint n)
{
    std::cerr << __FUNCTION__ << std::endl;
    for (;;)
    {
        SRV_C_AASSERT_TEXT(n > n * 2, "Assert for invalid condition");
    }
}

#pragma GCC diagnostic ignored "-Wdiv-by-zero"

void try_overflow(int n)
{
    std::cerr << __FUNCTION__ << std::endl;
    n /= 0;
}

void try_wrong_delete()
{
    std::cerr << __FUNCTION__ << std::endl;
    int wrong_del_;
    delete &wrong_del_;
}

void try_double_delete(int n)
{
    std::cerr << __FUNCTION__ << std::endl;
    auto* p = new int { n };
    delete p;
    delete p;
}

void try_wrong_pointer(int n)
{
    std::cerr << __FUNCTION__ << std::endl;
    auto* p = new int { n };
    delete p;
    *p = n;
}

void try_inf_recursive()
{
    std::cerr << __FUNCTION__ << std::endl;
    for (;;)
    {
        try_inf_recursive();
    }
}

static void try_wrong_delete_in_static_namespace()
{
    std::cerr << __FUNCTION__ << std::endl;
    try_wrong_delete();
}

namespace tests {
    class cls1
    {
    public:
        cls1(int n)
            : _n(n)
        {
        }
        virtual ~cls1() {}

        virtual int fail()
        {
            this->_fail();
            return _n;
        }

    protected:
        void _fail()
        {
            try_wrong_delete();
        }

        int _n = 0;
    };

    class cls2 : public cls1
    {
    public:
        using cls1::cls1;

        int fail() override
        {
            return this->cls1::fail();
        }

        virtual void fail2()
        {
            this->_fail();
        }

    private:
        void _fail()
        {
            try_wrong_pointer(4);
        }
    };

} // namespace tests
namespace { //anonymous

    class cls_anonymous : public tests::cls2
    {
    public:
        using cls2::cls2;

        int fail() override
        {
            return this->cls2::fail();
        }
    };

} // namespace

void try_wrong_delete_in_class()
{
    std::cerr << __FUNCTION__ << std::endl;

    tests::cls1 c { 1 };

    c.fail();
}

void try_wrong_pointer_in_inherited_class()
{
    std::cerr << __FUNCTION__ << std::endl;

    tests::cls2 c { 2 };

    c.fail2();
}

static void try_wrong_pointer_in_anonymous_namespace()
{
    std::cerr << __FUNCTION__ << std::endl;

    cls_anonymous c { 1 };

    c.fail();
}

void try_unhandled_exception()
{
    std::cerr << __FUNCTION__ << std::endl;

    throw std::logic_error("Test for unhandled_exception");
}

bool try_fail(const fail f)
{
    switch (f)
    {
    case fail::try_libassert:
        try_libassert(12);
        break;
    case fail::try_clibassert:
        try_clibassert(13);
        break;
    case fail::try_overflow:
        try_overflow(999);
        break;
    case fail::try_wrong_delete:
        try_wrong_delete();
        break;
    case fail::try_double_delete:
        try_double_delete(1);
        break;
    case fail::try_wrong_pointer:
        try_wrong_pointer(2);
        break;
    case fail::try_wrong_delete_in_lambda:
    {
        auto lmb_try = []() {
            try_wrong_delete();
        };
        lmb_try();
        break;
    }
    case fail::try_wrong_pointer_in_lambda:
    {
        auto lmb_try = []() {
            try_wrong_pointer(3);
        };
        lmb_try();
        break;
    }
    case fail::try_inf_recursive:
        try_inf_recursive();
        break;
    case fail::try_wrong_delete_in_static_namespace:
        try_wrong_delete_in_static_namespace();
        break;
    case fail::try_wrong_delete_in_class:
        try_wrong_delete_in_class();
        break;
    case fail::try_wrong_pointer_in_inherited_class:
        try_wrong_pointer_in_inherited_class();
        break;
    case fail::try_wrong_pointer_in_anonymous_namespace:
        try_wrong_pointer_in_anonymous_namespace();
        break;
    case fail::try_unhandled_exception:
        try_unhandled_exception();
        break;
    default:
        return false;
    }
    return true;
};

#define PRINT_FAIL_ID(fail_id) #fail_id

#define TRY_IF(fail_id)                       \
    if (fail == #fail_id)                     \
    {                                         \
        return try_fail(fail::try_##fail_id); \
    }

std::string get_fail_cases()
{
    static const char DELIM[] = "\t\n";
    std::stringstream ss;
    ss << DELIM
       << PRINT_FAIL_ID(libassert);
    ss << DELIM
       << PRINT_FAIL_ID(clibassert);
    ss << DELIM
       << PRINT_FAIL_ID(overflow);
    ss << DELIM
       << PRINT_FAIL_ID(wrong_delete);
    ss << DELIM
       << PRINT_FAIL_ID(double_delete);
    ss << DELIM
       << PRINT_FAIL_ID(wrong_pointer);
    ss << DELIM
       << PRINT_FAIL_ID(wrong_delete_in_lambda);
    ss << DELIM
       << PRINT_FAIL_ID(wrong_pointer_in_lambda);
    ss << DELIM
       << PRINT_FAIL_ID(inf_recursive);
    ss << DELIM
       << PRINT_FAIL_ID(wrong_delete_in_static_namespace);
    ss << DELIM
       << PRINT_FAIL_ID(wrong_delete_in_class);
    ss << DELIM
       << PRINT_FAIL_ID(wrong_pointer_in_inherited_class);
    ss << DELIM
       << PRINT_FAIL_ID(wrong_pointer_in_anonymous_namespace);
    ss << DELIM
       << PRINT_FAIL_ID(unhandled_exception);
    return ss.str();
}

bool try_fail(const std::string& fail)
{
    TRY_IF(libassert)
    TRY_IF(clibassert)
    TRY_IF(overflow)
    TRY_IF(wrong_delete)
    TRY_IF(double_delete)
    TRY_IF(wrong_pointer)
    TRY_IF(wrong_delete_in_lambda)
    TRY_IF(wrong_pointer_in_lambda)
    TRY_IF(inf_recursive)
    TRY_IF(wrong_delete_in_static_namespace)
    TRY_IF(wrong_delete_in_class)
    TRY_IF(wrong_pointer_in_inherited_class)
    TRY_IF(wrong_pointer_in_anonymous_namespace)
    TRY_IF(unhandled_exception)
    return false;
}
} // namespace server_lib
