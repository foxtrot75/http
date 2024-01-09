#pragma once

#include <string>

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include "message.hpp"

namespace http
{

namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = boost::beast::http;

class Server
    : public std::enable_shared_from_this<Server>
{
public:
    using CallbackType = std::function<Response(Request const&)>;

public:
    Server(asio::io_context& ioc);

    void setup(std::string_view host, uint16_t port);
    void setCallback(CallbackType callback);

    bool run();

private:
    void _accept();
    void _onAccept(beast::error_code ec, asio::ip::tcp::socket socket);

private:
    class Session
        : public std::enable_shared_from_this<Session>
    {
    public:
        Session(asio::ip::tcp::socket&& socket, CallbackType callback);

        void run();

    private:
        void _onRun();
        void _onRead(beast::error_code ec, std::size_t bytes_transferred);
        void _onWrite(beast::error_code ec, std::size_t bytes_transferred);

        void _processRequest();

        void _processError(beast::error_code const& ec, std::string_view msg);

    private:
        static constexpr uint               _version = 11;
        static constexpr uint64_t           _payloadLimit = 1024;

        beast::tcp_stream                   _stream;
        beast::flat_buffer                  _buffer;
        http::request<http::string_body>    _request;
        http::response<http::string_body>   _response;

        CallbackType                        _callback = nullptr;
    };

private:
    asio::io_context&       _ioc;
    asio::ip::tcp::acceptor _acceptor;
    asio::ip::tcp::endpoint _endpoint;

    CallbackType            _callback = nullptr;
};

}
