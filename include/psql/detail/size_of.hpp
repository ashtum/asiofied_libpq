#pragma once

#include <psql/detail/type_traits.hpp>

#include <chrono>
#include <string>
#include <string_view>

#include <boost/pfr.hpp>

namespace psql
{
namespace detail
{
template<class T>
struct size_of_impl;

template<typename T>
constexpr std::size_t size_of(const T& v)
{
  return size_of_impl<std::decay_t<T>>::apply(v);
}

template<typename T>
  requires(std::is_integral_v<T> || std::is_floating_point_v<T> || std::is_same_v<T, std::byte>)
struct size_of_impl<T>
{
  static constexpr std::size_t apply(const T&)
  {
    return sizeof(T);
  }
};

template<>
struct size_of_impl<std::chrono::system_clock::time_point>
{
  static constexpr std::size_t apply(const std::chrono::system_clock::time_point&)
  {
    return 8;
  }
};

template<>
struct size_of_impl<const char*>
{
  static constexpr std::size_t apply(const char* value)
  {
    return std::string_view{ value }.size();
  }
};

template<>
struct size_of_impl<std::string_view>
{
  static constexpr std::size_t apply(const std::string_view& value)
  {
    return value.size();
  }
};

template<>
struct size_of_impl<std::string>
{
  static constexpr std::size_t apply(const std::string& value)
  {
    return value.size();
  }
};

template<typename T>
  requires(is_array_v<T>)
struct size_of_impl<T>
{
  static constexpr std::size_t apply(const T& vec)
  {
    std::size_t size = 20;
    for (const auto& value : vec)
      size += size_of(value) + 4;
    return size;
  }
};

template<typename T>
  requires(is_composite_v<T>)
struct size_of_impl<T>
{
  static constexpr std::size_t apply(const T& value)
    requires(is_user_defined_v<T>)
  {
    std::size_t sum = 4;
    boost::pfr::for_each_field(value, [&](auto& f) { sum += size_of(f) + 8; });
    return sum;
  }

  static constexpr std::size_t apply(const T& value)
    requires(is_tuple_v<T>)
  {
    return 4 + std::apply([&](auto&&... ms) { return ((size_of(ms) + 8) + ...); }, value);
  }
};
} // namespace detail
} // namespace psql
