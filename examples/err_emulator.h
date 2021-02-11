#pragma once

#include <string>

namespace server_lib {

enum class fail
{
    try_none = 0,
    try_libassert, //6
    try_clibassert, //6
    try_unhandled_exception, //6
    try_overflow, //8 -> 6
    try_wrong_delete, //6
    try_double_delete, //6
    try_wrong_pointer, //11 -> 6
    try_deleted_pointer, //<- X_X
    try_uninitialized_pointer, //11 -> 6
    try_out_of_bound, //6
    try_improper_scanf, //11 -> 6
    try_inf_recursive, //11 -> 6
    try_fail_in_lambda,
    try_fail_in_static_namespace,
    try_fail_in_class,
    try_fail_in_inherited_class,
    try_fail_in_anonymous_namespace
};

bool try_fail(const fail);

std::string get_fail_cases();
bool try_fail(const std::string& fail);

} // namespace server_lib
