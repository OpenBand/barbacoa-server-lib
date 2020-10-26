#pragma once

#include <memory>

#define DECLARE_PTR_(ct, class_name)                                  \
    ct class_name;                                                    \
    typedef std::shared_ptr<class_name> class_name##_ptr;             \
    typedef std::weak_ptr<class_name> class_name##_wptr;              \
    typedef std::shared_ptr<const class_name> class_name##_const_ptr; \
    typedef std::weak_ptr<const class_name> class_name##_const_wptr;

#define DECLARE_PTR(class_name) DECLARE_PTR_(class, class_name)
#define DECLARE_PTR_S(class_name) DECLARE_PTR_(struct, class_name)
