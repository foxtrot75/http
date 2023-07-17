#include <thread>

#include <loguru.hpp>

#include "http/client.hpp"

namespace Http
{

Client::Client(asio::io_context& ioc)
    : _resolver(ioc)
    , _stream(ioc)
{}

void Client::setup(std::string const& host, uint16_t port)
{
    _host = host;
    _port = port;
}

void Client::setTimeout(std::chrono::seconds timeout)
{
    _timeout = timeout;
}

bool Client::get(Query const& query, std::string& response)
{
    std::scoped_lock lock(_mutex);
    _createRequest(query, http::verb::get);
    _run();

    _condition.wait(_mutex);

    if(_ec)
        return false;

    response = _response.body();
    return true;
}

bool Client::post(Query const& query, std::string& response)
{
    std::scoped_lock lock(_mutex);
    _createRequest(query, http::verb::post);
    _run();

    _condition.wait(_mutex);

    if(_ec)
        return false;

    response = _response.body();
    return true;
}

void Client::_createRequest(Query const& query, http::verb method)
{
    std::string target = query.target;
    if(query.params.size()) {
        target += "?";
        for(auto& [key, value] : query.params)
            target += key + "=" + value + "&";
        target.pop_back();
    }

    _request.version(_version);
    _request.method(method);
    _request.target(target);
    _request.set(http::field::host, _host);
    _request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    for(auto& [key, value] : query.fields)
        _request.set(key, value);

    _request.body() = query.body;
    _request.prepare_payload();

    LOG(debug) << _request.base()
               << (_request.payload_size() < _payloadLimit
                   ? _request.body()
                   : "");
}

void Client::_run()
{
    _resolver.async_resolve(
        _host.data(), std::to_string(_port).data(),
        beast::bind_front_handler(
            &Client::_onResolve,
            shared_from_this()));
}

void Client::_onResolve(beast::error_code ec,
                        asio::ip::tcp::resolver::results_type results)
{
    if(ec)
        return _processError(ec, "Resolve");

    _stream.expires_after(_timeout);

    // Make the connection on the IP address we get from a lookup
    _stream.async_connect(
        results,
        beast::bind_front_handler(
            &Client::_onConnect,
            shared_from_this()));
}

void Client::_onConnect(beast::error_code ec,
                        asio::ip::tcp::resolver::results_type::endpoint_type endpoint)
{
    boost::ignore_unused(endpoint);

    if(ec)
        return _processError(ec, "Connect");

    _stream.expires_after(_timeout);

    // Send the HTTP request to the remote host
    http::async_write(
        _stream, _request,
        beast::bind_front_handler(
            &Client::_onWrite,
            shared_from_this()));
}

void Client::_onWrite(beast::error_code ec,
                      size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    if(ec)
        return _processError(ec, "Write");

    _stream.expires_after(_timeout);

    // Receive the HTTP response
    http::async_read(
        _stream, _buffer, _response,
        beast::bind_front_handler(
            &Client::_onRead,
            shared_from_this()));
}

void Client::_onRead(beast::error_code ec,
                     size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    if(ec)
        return _processError(ec, "Read");

    LOG(debug) << _response.base()
               << (_response.payload_size() < _payloadLimit
                   ? _response.body()
                   : "");

    _condition.notify_all();

    _stream.socket().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
}

void Client::_processError(beast::error_code const& ec, std::string_view msg)
{
    std::this_thread::sleep_for(_errorTimeout);

    LOG(error) << msg << ": " << ec.message();
    _ec = ec;
    _condition.notify_all();
}

}
