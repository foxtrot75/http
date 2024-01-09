#include <loguru.hpp>

#include "http/server.hpp"

namespace http
{

Server::Server(asio::io_context& ioc)
    : _ioc(ioc)
    , _acceptor(ioc)
{}

void Server::setup(std::string_view host, uint16_t port)
{
    asio::ip::address address = asio::ip::make_address(host);
    _endpoint = asio::ip::tcp::endpoint(address, port);
}

void Server::setCallback(CallbackType callback)
{
    _callback = callback;
}

bool Server::run()
{
    beast::error_code ec;

    auto processError = [](beast::error_code const& ec, std::string_view msg) -> bool {
        LOG(error) << msg << ": " << ec.message();
        return false;
    };

    _acceptor.open(_endpoint.protocol(), ec);
    if(ec)
        return processError(ec, "Open error");

    _acceptor.set_option(asio::socket_base::reuse_address(true), ec);
    if(ec)
        return processError(ec, "Set option reuse error");

    _acceptor.bind(_endpoint, ec);
    if(ec)
        return processError(ec, "Bind error");

    _acceptor.listen(asio::socket_base::max_listen_connections, ec);
    if(ec)
        return processError(ec, "Listen error");

    _accept();

    return true;
}

void Server::_accept()
{
    _acceptor.async_accept(
        asio::make_strand(_ioc),
        beast::bind_front_handler(
            &Server::_onAccept,
            shared_from_this()));
}

void Server::_onAccept(beast::error_code ec, asio::ip::tcp::socket socket)
{
    if(ec) {
        LOG(error) << "Accept error: " << ec.message();
        return;
    }

    std::make_shared<Session>(std::move(socket), _callback)->run();

    _accept();
}


Server::Session::Session(asio::ip::tcp::socket&& socket, CallbackType callback)
    : _stream(std::move(socket))
    , _callback(callback)
{}

void Server::Session::run()
{
    asio::dispatch(
        _stream.get_executor(),
        beast::bind_front_handler(
            &Session::_onRun,
            shared_from_this()));
}

void Server::Session::_onRun()
{
    _stream.expires_after(std::chrono::seconds(10));

    http::async_read(
        _stream, _buffer, _request,
        beast::bind_front_handler(
            &Session::_onRead,
            shared_from_this()));
}

void Server::Session::_onRead(beast::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    if(ec)
        return _processError(ec, "Read");

    _processRequest();

    http::async_write(
        _stream, _response,
        beast::bind_front_handler(
            &Session::_onWrite,
            shared_from_this()));
}

void Server::Session::_onWrite(beast::error_code ec, std::size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    if(ec)
        return _processError(ec, "Write");

    _stream.socket().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
}

void Server::Session::_processRequest()
{
    LOG(debug)
        << _request.base()
        << (_request.payload_size() < _payloadLimit
            ? _request.body()
            : "");

    Request req;
    req.target = std::string(_request.target());
    for(auto&& f: _request) {
        std::string key(f.name_string());
        std::string value(f.value());
        std::transform(key.begin(), key.end(), key.begin(), [](char c) { return std::tolower(c); });
        req.fields[key] = value;
    }
    req.body = _request.body();

    Response rsp;
    if(_callback)
        rsp = _callback(req);

    _response.version(_version);
    _response.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    _response.result(rsp.result);

    for(auto& [key, value]: rsp.fields)
        _response.set(key, value);

    if(!rsp.body.empty()) {
        _response.body() = rsp.body;
        _response.prepare_payload();
    }

    LOG(debug)
        << _response.base()
        << (_response.payload_size() < _payloadLimit
            ? _response.body()
            : "");
}

void Server::Session::_processError(beast::error_code const& ec, std::string_view msg)
{
    LOG(error) << msg << ": " << ec.message();
}

}
