#ifndef ETWP_MACROS_HPP
#define ETWP_MACROS_HPP

#define ETWP_WIDEN_IMPL(x) L ## x
#define ETWP_WIDEN(x) ETWP_WIDEN_IMPL(x)

#define ETWP_WFILE ETWP_WIDEN(__FILE__)
#define ETWP_WFUNC ETWP_WIDEN(__FUNCTION__)

#define ETWP_EXPAND_AND_STRINGIFY_IMPL(param) #param
#define ETWP_EXPAND_AND_STRINGIFY(param) ETWP_EXPAND_AND_STRINGIFY_IMPL(param)

#define ETWP_EXPAND_AND_STRINGIFY_WIDE_IMPL(param) ETWP_WIDEN_IMPL(#param)
#define ETWP_EXPAND_AND_STRINGIFY_WIDE(param) ETWP_EXPAND_AND_STRINGIFY_WIDE_IMPL(param)

#define ETWP_UNUSED_VARIABLE(var) (void)(var)

#define ETWP_DISABLE_COPY_AND_MOVE(classname)           \
classname (const classname&) = delete;                  \
const classname& operator= (const classname&) = delete; \
classname (classname&&) = delete;                       \
classname& operator= (classname&&) = delete;            \


#endif  // #ifndef ETWP_MACROS_HPP