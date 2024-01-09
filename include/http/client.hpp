#pragma once

#include <string>

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include "message.hpp"

using namespace std::chrono_literals;

namespace http
{

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = boost::beast::http;

class Client
    : public std::enable_shared_from_this<Client>
{
public:
    Client(asio::io_context& ioc);

    void setup(std::string_view host, uint16_t port);
    void setTimeout(std::chrono::seconds timeout);

    bool get(Request const& request, std::string& response);
    bool post(Request const& request, std::string& response);

private:
    void _createRequest(Request const& request, http::verb method);

    void _run();
    void _onResolve(
        beast::error_code ec,
        asio::ip::tcp::resolver::results_type results);
    void _onConnect(
        beast::error_code ec,
        asio::ip::tcp::resolver::results_type::endpoint_type endpoint);
    void _onWrite(beast::error_code ec, std::size_t bytes_transferred);
    void _onRead(beast::error_code ec, std::size_t bytes_transferred);

    void _processError(beast::error_code const& ec, std::string_view msg);

private:
    static constexpr auto                       _defaultTimeout = 10s;
    static constexpr auto                       _errorTimeout = 100ms;
    static constexpr uint                       _version = 11;
    static constexpr uint64_t                   _payloadLimit = 2048;

    asio::ip::tcp::resolver                     _resolver;
    beast::tcp_stream                           _stream;
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
