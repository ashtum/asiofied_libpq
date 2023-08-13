#include <asiofiedpq/connection.hpp>

#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/io_context.hpp>

#include <iostream>

namespace asio = boost::asio;

asio::awaitable<void> run_exmaple(asiofiedpq::connection& conn);

asio::awaitable<void> async_main()
{
  using namespace asio::experimental::awaitable_operators;

  auto conn = asiofiedpq::connection{ co_await asio::this_coro::executor };

  co_await conn.async_connect("postgresql://postgres:postgres@172.18.0.2:5432", asio::deferred);

  co_await (conn.async_run(asio::use_awaitable) || run_exmaple(conn));
}

int main()
{
  try
  {
    auto ioc = asio::io_context{};

    asio::co_spawn(
      ioc,
      async_main(),
      [](const std::exception_ptr& ep)
      {
        if (ep)
          std::rethrow_exception(ep);
      });

    ioc.run();
  }
  catch (const std::exception& e)
  {
    std::cout << "exception:" << e.what() << std::endl;
  }
}