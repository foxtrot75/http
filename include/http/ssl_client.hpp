#pragma once

#include <string>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>

#include "query.hpp"

namespace Http
{

using namespace std::chrono_literals;

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = boost::beast::http;

class SslClient
    : public std::enable_shared_from_this<SslClient>
{
public:
    explicit SslClient(asio::io_context& ioc, asio::ssl::context& ctx);

    void setup(std::string const& host, uint16_t port);
    void setTimeout(std::chrono::seconds timeout);

    bool get(Query const& query, std::string& response);
    bool post(Query const& query, std::string& response);

private:
    void _createRequest(Query const& query, http::verb method);

    void _run();
    void _onResolve(beast::error_code ec, asio::ip::tcp::resolver::results_type results);
    void _onConnect(beast::error_code ec, asio::ip::tcp::resolver::results_type::endpoint_type endpoint);
    void _onHandshake(beast::error_code ec);
    void _onWrite(beast::error_code ec, size_t bytes_transferred);
    void _onRead(beast::error_code ec, size_t bytes_transferred);
    void _onShutdown(beast::error_code ec);

    void _processError(beast::error_code const& ec, std::string_view msg);

private:
    static constexpr auto                       _defaultTimeout = 10s;
    static constexpr auto                       _errorTimeout = 100ms;
    static constexpr uint                       _version = 11;
    static constexpr uint64_t                   _payloadLimit = 10240;

    asio::ip::tcp::resolver                     _resolver;
    beast::ssl_stream<beast::tcp_stream>        _stream;
    beast::flat_buffer                          _buffer;
    beast::error_code                           _ec;
    http::request<http::string_body>            _request;
    http::response<http::string_body>           _response;

    std::string                                 _host;
    uint16_t                                    _port;
    std::chrono::seconds                        _timeout = _defaultTimeout;

    std::mutex                                  _mutex;
    std::condition_variable_any                 _condition;
};

}
