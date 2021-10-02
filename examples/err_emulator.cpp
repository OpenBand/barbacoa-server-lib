#include "err_emulator.h"

#include <server_lib/asserts.h>
#include <server_clib/macro.h>

#include <stdio.h>
#include <iostream>
#include <sstream>

namespace server_lib {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Warray-bounds"
#pragma GCC diagnostic ignored "-Wunused-result"

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

void try_unhandled_exception()
{
    std::cerr << __FUNCTION__ << std::endl;

    throw std::logic_error("Test for unhandled_exception");
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
    *((int*)0xff00000000000000) = 0xff;
}

void try_deleted_pointer(uint64_t n)
{
    std::cerr << __FUNCTION__ << std::endl;
    auto* p = new uint64_t { n };
    delete p;
    auto* p2 = new uint64_t { *p };
    *p2 = n + 1;
    assertm(*p != n, "?");
    *p = n;
    assertm(*p == n, "?");
}

void try_uninitialized_pointer(int limit)
{
    std::cerr << __FUNCTION__ << std::endl;
    int* p;
    for (size_t ci = 0; ci < limit; ++ci)
    {
        auto search_pos = ci + 1;
        printf("%d->%d->", *(p += search_pos), *p);
        if (ci % 6 == 0)
            printf("x\n");
    }
}

void try_out_of_bound(int n)
{
    std::cerr << __FUNCTION__ << std::endl;
    int arr[2];
    arr[3] = n; // accessing out of bound
}

void try_improper_scanf(int n)
{
    std::cerr << __FUNCTION__ << std::endl;
    printf("Enter number: ");
    scanf("%d", n);
}

void try_inf_recursive()
{
    std::cerr << __FUNCTION__ << std::endl;
    for (;;)
    {
        try_inf_recursive();
    }
}

void try_fail()
{
    try_wrong_delete();
}

static void try_fail_in_static_namespace()
{
    std::cerr << __FUNCTION__ << std::endl;
    try_fail();
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
            try_fail();
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
            try_fail();
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

void try_fail_in_class()
{
    std::cerr << __FUNCTION__ << std::endl;

    tests::cls1 c { 1 };

    c.fail();
}

void try_fail_in_inherited_class()
{
    std::cerr << __FUNCTION__ << std::endl;

    tests::cls2 c { 2 };

    c.fail2();
}

static void try_fail_in_anonymous_namespace()
{
    std::cerr << __FUNCTION__ << std::endl;

    cls_anonymous c { 1 };

    c.fail();
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
    case fail::try_unhandled_exception:
        try_unhandled_exception();
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
    case fail::try_deleted_pointer:
        try_deleted_pointer(2);
        break;
    case fail::try_uninitialized_pointer:
        try_uninitialized_pointer(0xffff);
        break;
    case fail::try_out_of_bound:
        try_out_of_bound(20);
        break;
    case fail::try_improper_scanf:
        try_improper_scanf(30);
        break;
    case fail::try_inf_recursive:
        try_inf_recursive();
        break;
    case fail::try_fail_in_lambda: {
        auto lmb_try = []() {
            try_fail();
        };
        lmb_try();
        break;
    }
    case fail::try_fail_in_static_namespace:
        try_fail_in_static_namespace();
        break;
    case fail::try_fail_in_class:
        try_fail_in_class();
        break;
    case fail::try_fail_in_inherited_class:
        try_fail_in_inherited_class();
        break;
    case fail::try_fail_in_anonymous_namespace:
        try_fail_in_anonymous_namespace();
        break;
    default:
        return false;
    }
    return true;
};

#pragma GCC diagnostic pop

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
       << PRINT_FAIL_ID(unhandled_exception);
    ss << DELIM
       << PRINT_FAIL_ID(overflow);
    ss << DELIM
       << PRINT_FAIL_ID(wrong_delete);
    ss << DELIM
       << PRINT_FAIL_ID(double_delete);
    ss << DELIM
       << PRINT_FAIL_ID(wrong_pointer);
    ss << DELIM
       << PRINT_FAIL_ID(deleted_pointer);
    ss << DELIM
       << PRINT_FAIL_ID(uninitialized_pointer);
    ss << DELIM
       << PRINT_FAIL_ID(out_of_bound);
    ss << DELIM
       << PRINT_FAIL_ID(improper_scanf);
    ss << DELIM
       << PRINT_FAIL_ID(inf_recursive);
    ss << DELIM
       << PRINT_FAIL_ID(fail_in_lambda);
    ss << DELIM
       << PRINT_FAIL_ID(fail_in_static_namespace);
    ss << DELIM
       << PRINT_FAIL_ID(fail_in_class);
    ss << DELIM
       << PRINT_FAIL_ID(fail_in_inherited_class);
    ss << DELIM
       << PRINT_FAIL_ID(fail_in_anonymous_namespace);
    return ss.str();
}

bool try_fail(const std::string& fail)
{
    TRY_IF(libassert)
    TRY_IF(clibassert)
    TRY_IF(unhandled_exception)
    TRY_IF(overflow)
    TRY_IF(wrong_delete)
    TRY_IF(double_delete)
    TRY_IF(wrong_pointer)
    TRY_IF(deleted_pointer)
    TRY_IF(uninitialized_pointer)
    TRY_IF(out_of_bound)
    TRY_IF(improper_scanf)
    TRY_IF(inf_recursive)
    TRY_IF(fail_in_lambda)
    TRY_IF(fail_in_static_namespace)
    TRY_IF(fail_in_class)
    TRY_IF(fail_in_inherited_class)
    TRY_IF(fail_in_anonymous_namespace)
    return false;
}
} // namespace server_lib
