#include <psql/connection.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/deferred.hpp>

#include <iostream>

namespace asio = boost::asio;

struct Employee
{
  std::string name;
  std::string phone;
};

struct Company
{
  std::int64_t id;
  std::vector<Employee> employees;
};

template<>
struct psql::user_defined<Employee>
{
  static constexpr auto name = "employee";
};

template<>
struct psql::user_defined<Company>
{
  static constexpr auto name = "company";
};

asio::awaitable<void> async_main(std::string conninfo)
{
  auto exec = co_await asio::this_coro::executor;
  auto conn = psql::connection{ exec };

  co_await conn.async_connect(conninfo, asio::deferred);

  // Creating user-defined types within the server.
  co_await conn.async_query("DROP TYPE IF EXISTS company;", asio::deferred);
  co_await conn.async_query("DROP TYPE IF EXISTS employee;", asio::deferred);
  co_await conn.async_query("CREATE TYPE employee AS (name TEXT, phone TEXT);", asio::deferred);
  co_await conn.async_query("CREATE TYPE company AS (id INT8, employees employee[]);", asio::deferred);

  auto company = Company{ 104, { { "Jane Eyre", "555-123-4567" }, { "Tom Hanks", "555-987-6543" } } };

  // The connection will query the Oid of user-defined types through the provided name in the specialization of
  // user_defined<>. These Oids will be stored within the connection, eliminating the need for future queries to
  // retrieve Oid values.
  auto result = co_await conn.async_query("SELECT $1;", psql::mp(company), asio::deferred);

  // Deserializes the result as a user-defined type.
  auto [id, employees] = as<Company>(result);

  std::cout << "company id:" << id << std::endl;

  std::cout << "company employees:" << std::endl;
  for (const auto& e : employees)
    std::cout << "name:" << e.name << '\t' << "phone:" << e.phone << '\t' << std::endl;
}
