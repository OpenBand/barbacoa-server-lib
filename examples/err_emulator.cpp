#include "err_emulator.h"

#include <server_clib/macro.h>

#include <strstream>

namespace server_lib {

void try_wrong_pointer() //throw SIGSEGV
{
    *((int*)0xff00000000000000) = 0xff;
}

void try_wrong_delete() //throw SIGABRT
{
    int wrong_del_;
    delete &wrong_del_;
}

void try_double_delete() //throw SIGABRT
{
    auto* p = new int { 10 };
    delete p;
    delete p;
}

void try_cerr(int n) //throw SIGABRT
{
    if (n < 0) //wroten some mess code to avoid compiler optimization
        return;
    for (;;)
    {
        SRV_C_AASSERT_TEXT(1 > 2, "Assert for invalid condition");
    }
}

#pragma GCC diagnostic ignored "-Wdiv-by-zero"

void try_overflow(int v) //throw SIGFPE
{
    v /= 0;
}

void try_inf_recursive()
{
    for (;;)
    {
        try_inf_recursive();
    }
}

static void try_wrong_delete_in_static_namespace()
{
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
            try_wrong_pointer();
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
    tests::cls1 c { 1 };

    c.fail();
}

void try_wrong_pointer_in_inherited_class()
{
    tests::cls2 c { 2 };

    c.fail2();
}

static void try_wrong_pointer_in_anonymous_namespace()
{
    cls_anonymous c { 1 };

    c.fail();
}

bool try_fail(const fail f)
{
    switch (f)
    {
    case fail::try_cerr:
        try_cerr(12);
        break;
    case fail::try_overflow:
        try_overflow(13);
        break;
    case fail::try_wrong_delete:
        try_wrong_delete();
        break;
    case fail::try_double_delete:
        try_double_delete();
        break;
    case fail::try_wrong_pointer:
        try_wrong_pointer();
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
            try_wrong_pointer();
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
    std::strstream ss;
    ss << PRINT_FAIL_ID(cerr) << " ";
    ss << PRINT_FAIL_ID(overflow) << " ";
    ss << PRINT_FAIL_ID(wrong_delete) << " ";
    ss << PRINT_FAIL_ID(double_delete) << " ";
    ss << PRINT_FAIL_ID(wrong_pointer) << " ";
    ss << PRINT_FAIL_ID(wrong_delete_in_lambda) << " ";
    ss << PRINT_FAIL_ID(wrong_pointer_in_lambda) << " ";
    ss << PRINT_FAIL_ID(inf_recursive) << " ";
    ss << PRINT_FAIL_ID(wrong_delete_in_static_namespace) << " ";
    ss << PRINT_FAIL_ID(wrong_delete_in_class) << " ";
    ss << PRINT_FAIL_ID(wrong_pointer_in_inherited_class) << " ";
    ss << PRINT_FAIL_ID(wrong_pointer_in_anonymous_namespace);
    return ss.str();
}

bool try_fail(const std::string& fail)
{
    TRY_IF(cerr)
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
    return false;
}
} // namespace server_lib
