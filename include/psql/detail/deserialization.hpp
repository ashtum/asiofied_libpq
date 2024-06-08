#pragma once

#include <psql/detail/oid_of.hpp>
#include <psql/detail/size_of.hpp>

#include <boost/endian.hpp>
#include <boost/pfr.hpp>

#include <span>

namespace psql
{
namespace detail
{
template<class T>
struct deserialize_impl;

template<typename T>
void deserialize(std::span<const char> buffer, T& v)
{
  deserialize_impl<std::decay_t<T>>::apply(buffer, v);
}

template<typename T>
  requires(std::is_integral_v<T> || std::is_floating_point_v<T> || std::is_same_v<T, std::byte>)
struct deserialize_impl<T>
{
  static void apply(std::span<const char> buffer, T& value)
  {
    value = boost::endian::endian_load<T, sizeof(T), boost::endian::order::big>(
      reinterpret_cast<const unsigned char*>(buffer.data()));
  }
};

template<>
struct deserialize_impl<std::chrono::system_clock::time_point>
{
  static void apply(std::span<const char> buffer, std::chrono::system_clock::time_point& value)
  {
    int64_t int_value{};
    deserialize(buffer, int_value);
    value = std::chrono::system_clock::time_point{} + std::chrono::microseconds{ int_value + 946684800000000 };
  }
};

template<>
struct deserialize_impl<std::string_view>
{
  static void apply(std::span<const char> buffer, std::string_view& value)
  {
    value = { buffer.begin(), buffer.end() };
  }
};

template<>
struct deserialize_impl<std::string>
{
  static void apply(std::span<const char> buffer, std::string& value)
  {
    value.append(buffer.begin(), buffer.end());
  }
};

inline void deserialize_and_verify_oid(std::span<const char> buffer, uint32_t expected_oid)
{
  uint32_t oid = {};
  deserialize<uint32_t>(buffer, oid);

  if (expected_oid != 0 && expected_oid != oid)
    throw std::runtime_error{ "Mismatched Object Identifiers (OIDs) in received and expected types. Found " +
                              std::to_string(oid) + " instead of " + std::to_string(expected_oid) };
}

template<typename T>
  requires(is_composite_v<T>)
struct deserialize_impl<T>
{
  template<typename U>
  static void deserialize_member(std::span<const char>& buffer, U& value)
  {
    deserialize_and_verify_oid(buffer, oid_of<U>());

    int32_t member_size = {};
    deserialize<int32_t>(buffer.subspan(4), member_size);

    deserialize(buffer.subspan(8, member_size), value);

    buffer = buffer.subspan(8 + member_size); // consumes buffer
  }

  static void verify_member_counts(std::span<const char>& buffer, int32_t expected_count)
  {
    int32_t count = {};
    deserialize<int32_t>(buffer, count);

    if (expected_count != count)
      throw std::runtime_error{ "Mismatched member counts in received and expected composite types. Found " +
                                std::to_string(count) + " instead of " + std::to_string(expected_count) };
  }

  static void apply(std::span<const char> buffer, T& value)
    requires(is_user_defined_v<T>)
  {
    verify_member_counts(buffer, boost::pfr::tuple_size_v<T>);
    buffer = buffer.subspan(4);
    boost::pfr::for_each_field(value, [&](auto& f) { deserialize_member(buffer, f); });
  }

  static void apply(std::span<const char> buffer, T& value)
    requires(is_tuple_v<T>)
  {
    verify_member_counts(buffer, std::tuple_size_v<T>);
    buffer = buffer.subspan(4);
    std::apply([&](auto&&... ms) { (deserialize_member(buffer, ms), ...); }, value);
  }
};

template<typename T>
  requires(is_array_v<T>)
struct deserialize_impl<T>
{
  using value_type = std::decay_t<typename T::value_type>;

  static void apply(std::span<const char> buffer, T& array)
  {
    int32_t dimensions_count = {};
    deserialize<int32_t>(buffer.subspan(0), dimensions_count);

    if (dimensions_count != 1)
      throw std::runtime_error{ "Unexpected multidimensional array" };

    deserialize_and_verify_oid(buffer.subspan(8), oid_of<value_type>());

    int32_t size = {};
    deserialize<int32_t>(buffer.subspan(12), size);

    buffer = buffer.subspan(20);
    array.resize(size);

    for (auto& value : array)
    {
      int32_t value_size = {};
      deserialize<int32_t>(buffer, value_size);
      deserialize(buffer.subspan(4, value_size), value);
      buffer = buffer.subspan(4 + value_size); // consumes buffer
    }
  }
};
} // namespace detail
} // namespace psql
