#include <psql/connection.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/deferred.hpp>

#include <iostream>

namespace asio = boost::asio;

asio::awaitable<void> run_exmaple(psql::connection& conn)
{
  const auto results = co_await conn.async_exec_pipeline(
    [](psql::pipeline& p)
    {
      p.push_query("DROP TABLE IF EXISTS phonebook;");
      p.push_query("CREATE TABLE phonebook(phone TEXT, name TEXT);");
      p.push_query("INSERT INTO phonebook VALUES ($1, $2);", std::tuple{ "+1 111 444 7777", "Jake" });
      p.push_query("INSERT INTO phonebook VALUES ($1, $2);", std::tuple{ "+2 333 222 3333", "Megan" });
      p.push_query("SELECT * FROM phonebook ORDER BY name;");
    },
    asio::deferred);

  for (const auto row : results.back())
  {
    const auto [phone, name] = as<std::string_view, std::string_view>(row);
    std::cout << name << ":" << phone << std::endl;
  }
}
