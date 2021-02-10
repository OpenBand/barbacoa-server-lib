#pragma once

#include <string>

namespace server_lib {

enum class fail
{
    try_none = 0,
    try_libassert,
    try_clibassert,
    try_overflow,
    try_wrong_delete,
    try_double_delete,
    try_wrong_pointer,
    try_wrong_delete_in_lambda,
    try_wrong_pointer_in_lambda,
    try_inf_recursive,
    try_wrong_delete_in_static_namespace,
    try_wrong_delete_in_class,
    try_wrong_pointer_in_inherited_class,
    try_wrong_pointer_in_anonymous_namespace,
    try_unhandled_exception
};

bool try_fail(const fail);

std::string get_fail_cases();
bool try_fail(const std::string& fail);

} // namespace server_lib
