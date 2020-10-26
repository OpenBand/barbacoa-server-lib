#include "err_emulator.h"

#include <server_clib/macro.h>

namespace server_lib {

void try_1() //throw SIGABRT
{
    int wrong_del_;
    delete &wrong_del_;
}

void try_2() //throw SIGSEGV
{
    *((int*)0xff00000000000000) = 0xff;
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

void try_infrec()
{
    for (;;)
    {
        try_infrec();
    }
}

static void try_static_namespace()
{
    try_1();
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
            try_1();
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
            try_2();
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

void try_cls1()
{
    tests::cls1 c { 1 };

    c.fail();
}

void try_cls2()
{
    tests::cls2 c { 2 };

    c.fail2();
}

static void try_cls_anonymous()
{
    cls_anonymous c { 1 };

    c.fail();
}

void try_fail(const fail f)
{
    switch (f)
    {
    case fail::tryc_err:
        try_cerr(12);
        break;
    case fail::tryc_overflow:
        try_overflow(13);
        break;
    case fail::try0x_1:
        try_1();
        break;
    case fail::try0x_2:
        try_2();
        break;
    case fail::try11_1:
    {
        auto lmb_try = []() {
            try_1();
        };
        lmb_try();
        break;
    }
    case fail::try11_2:
    {
        auto lmb_try = []() {
            try_2();
        };
        lmb_try();
        break;
    }
    case fail::try_infrec:
        try_infrec();
        break;
    case fail::try_static_namespace:
        try_static_namespace();
        break;
    case fail::try_cls1:
        try_cls1();
        break;
    case fail::try_cls2:
        try_cls2();
        break;
    case fail::try_cls_anonymous:
        try_cls_anonymous();
        break;
    default:;
    }
};

} // namespace server_lib
