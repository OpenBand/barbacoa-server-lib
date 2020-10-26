#pragma once

namespace server_lib {

enum class fail
{
    try_none = 0,
    tryc_err,
    tryc_overflow,
    try0x_1,
    try0x_2,
    try11_1,
    try11_2,
    try_infrec, //!!!
    try_static_namespace,
    try_cls1,
    try_cls2,
    try_cls_anonymous
};

void try_fail(const fail);

} // namespace server_lib
