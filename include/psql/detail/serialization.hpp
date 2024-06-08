#pragma once

#include <psql/detail/oid_of.hpp>
#include <psql/detail/size_of.hpp>
#include <psql/params.hpp>

#include <boost/endian.hpp>
#include <boost/pfr.hpp>

#include <array>

namespace psql
{
namespace detail
{
template<class T>
struct serialize_impl;

template<typename T>
const char* serialize(const oid_map& omp, std::string& buffer, const T& v)
{
  const char* ret = &buffer.front() + buffer.size();
  serialize_impl<std::decay_t<T>>::apply(omp, buffer, v);
  return ret;
}

template<typename... Ts>
auto serialize(const oid_map& omp, std::string& buffer, const params<Ts...>& params)
{
  struct result_type
  {
    std::array<uint32_t, sizeof...(Ts)> types;
    std::array<const char*, sizeof...(Ts)> values;
    std::array<int, sizeof...(Ts)> lengths;
    std::array<int, sizeof...(Ts)> formats;
  };

  return std::apply(
    [&](const auto&... args)
    {
      buffer.clear();
      buffer.reserve((0 + ... + size_of(args)));

      return result_type{ { oid_of<decltype(args)>(omp)... },
                          { serialize(omp, buffer, args)... },
                          { static_cast<int>(size_of(args))... },
                          { ((void)args, true)... } };
    },
    static_cast<const std::tuple<Ts...>&>(params));
}

template<typename T>
  requires(std::is_integral_v<T> || std::is_floating_point_v<T> || std::is_same_v<T, std::byte>)
struct serialize_impl<T>
{
  static void apply(const oid_map&, std::string& buffer, const T& value)
  {
    buffer.resize(buffer.size() + sizeof(T));
    auto* p = reinterpret_cast<unsigned char*>(std::addressof(buffer.back()) - (sizeof(T) - 1));
    boost::endian::endian_store<T, sizeof(T), boost::endian::order::big>(p, value);
  }
};

template<>
struct serialize_impl<std::chrono::system_clock::time_point>
{
  static void apply(const oid_map& omp, std::string& buffer, const std::chrono::system_clock::time_point& value)
  {
    const int64_t int_value = (std::chrono::duration_cast<std::chrono::microseconds>(value.time_since_epoch()) -
                               std::chrono::microseconds{ 946684800000000 })
                                .count();
    serialize(omp, buffer, int_value);
  }
};

template<>
struct serialize_impl<const char*>
{
  static void apply(const oid_map&, std::string& buffer, const char* value)
  {
    buffer.append(value);
  }
};

template<>
struct serialize_impl<std::string_view>
{
  static void apply(const oid_map&, std::string& buffer, const std::string_view& value)
  {
    buffer.append(value);
  }
};

template<>
struct serialize_impl<std::string>
{
  static void apply(const oid_map&, std::string& buffer, const std::string& value)
  {
    buffer.append(value);
  }
};

template<typename T>
  requires(is_composite_v<T>)
struct serialize_impl<T>
{
  template<typename U>
  static void serialize_member(const oid_map& omp, std::string& buffer, const U& value)
  {
    serialize<int32_t>(omp, buffer, oid_of<U>(omp));
    serialize<int32_t>(omp, buffer, size_of(value));
    serialize(omp, buffer, value);
  }

  static void apply(const oid_map& omp, std::string& buffer, const T& value)
    requires(is_user_defined_v<T>)
  {
    serialize<int32_t>(omp, buffer, boost::pfr::tuple_size_v<T>);
    boost::pfr::for_each_field(value, [&](const auto& f) { serialize_member(omp, buffer, f); });
  }

  static void apply(const oid_map& omp, std::string& buffer, const T& value)
    requires(is_tuple_v<T>)
  {
    serialize<int32_t>(omp, buffer, std::tuple_size_v<T>);
    std::apply([&](auto&&... ms) { (serialize_member(omp, buffer, ms), ...); }, value);
  }
};

template<typename T>
  requires(is_array_v<T>)
struct serialize_impl<T>
{
  using value_type = std::decay_t<typename T::value_type>;

  static void apply(const oid_map& omp, std::string& buffer, const T& array)
  {
    serialize<int32_t>(omp, buffer, 1);
    serialize<int32_t>(omp, buffer, 0);
    serialize<int32_t>(omp, buffer, oid_of<value_type>(omp));
    serialize<int32_t>(omp, buffer, std::size(array));
    serialize<int32_t>(omp, buffer, 0);

    for (const auto& value : array)
    {
      serialize<int32_t>(omp, buffer, size_of(value));
      serialize(omp, buffer, value);
    }
  }
};
} // namespace detail
} // namespace psql
