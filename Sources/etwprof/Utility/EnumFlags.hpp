#ifndef ETWP_ENUM_FLAGS_HPP
#define ETWP_ENUM_FLAGS_HPP

#include <type_traits>

#define ETWP_ENUM_FLAG_SUPPORT(T)                                                                                                               \
static_assert (std::is_scoped_enum<T>::value, "Flag support can only be enabled for (strongly typed) enums!");                                  \
                                                                                                                                                \
[[maybe_unused]] constexpr inline T operator~ (T v) { return T (~std::underlying_type<T>::type (v)); }                                          \
[[maybe_unused]] constexpr inline T operator| (T l, T r) { return T (std::underlying_type<T>::type (l) | std::underlying_type<T>::type (r)); }  \
[[maybe_unused]] constexpr inline T operator& (T l, T r) { return T (std::underlying_type<T>::type (l) & std::underlying_type<T>::type (r)); }  \
[[maybe_unused]] constexpr inline T operator^ (T l, T r) { return T (std::underlying_type<T>::type (l) ^ std::underlying_type<T>::type (r)); }  \
                                                                                                                                                \
[[maybe_unused]] constexpr inline T& operator|= (T& l, T r) { l = l | r; return l; }                                                            \
[[maybe_unused]] constexpr inline T& operator&= (T& l, T r) { l = l & r; return l; }                                                            \
[[maybe_unused]] constexpr inline T& operator^= (T& l, T r) { l = l ^ r; return l; }                                                            \

#define ETWP_ENUM_FLAG_SUPPORT_CLASS(T)                                                                                                               \
static_assert (std::is_scoped_enum<T>::value, "Flag support can only be enabled for (strongly typed) enums!");                                  \
                                                                                                                                                \
[[maybe_unused]] friend constexpr inline T operator~ (T v) { return T (~std::underlying_type<T>::type (v)); }                                          \
[[maybe_unused]] friend constexpr inline T operator| (T l, T r) { return T (std::underlying_type<T>::type (l) | std::underlying_type<T>::type (r)); }  \
[[maybe_unused]] friend constexpr inline T operator& (T l, T r) { return T (std::underlying_type<T>::type (l) & std::underlying_type<T>::type (r)); }  \
[[maybe_unused]] friend constexpr inline T operator^ (T l, T r) { return T (std::underlying_type<T>::type (l) ^ std::underlying_type<T>::type (r)); }  \
                                                                                                                                                \
[[maybe_unused]] friend constexpr inline T& operator|= (T& l, T r) { l = l | r; return l; }                                                            \
[[maybe_unused]] friend constexpr inline T& operator&= (T& l, T r) { l = l & r; return l; }                                                            \
[[maybe_unused]] friend constexpr inline T& operator^= (T& l, T r) { l = l ^ r; return l; }                                                            \

template<typename T>
inline bool IsFlagSet (T v, T f) { return std::underlying_type<T>::type(v) & std::underlying_type<T>::type(f); }

#endif  // #ifndef ETWP_ENUM_FLAGS_HPP