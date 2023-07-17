#include <sstream>
#include <thread>

#include <loguru.hpp>

#include "http/ssl_client.hpp"

namespace Http
{

SslClient::SslClient(asio::io_context& ioc, asio::ssl::context& ctx)
    : _resolver(ioc)
    , _stream(ioc, ctx)
{}

void SslClient::setup(std::string const& host, uint16_t port)
{
    _host = host;
    _port = port;
}

void SslClient::setTimeout(std::chrono::seconds timeout)
{
    _timeout = timeout;
}

bool SslClient::get(Query const& query, std::string& response)
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

bool SslClient::post(Query const& query, std::string& response)
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

void SslClient::_createRequest(Query const& query, http::verb method)
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

void SslClient::_run()
{
    if(!SSL_set_tlsext_host_name(_stream.native_handle(), _host.data())) {
        beast::error_code ec((int)ERR_get_error(), asio::error::get_ssl_category());
        _processError(ec, "Ssl set:");
    }

    _resolver.async_resolve(
        _host.data(), std::to_string(_port).data(),
        beast::bind_front_handler(
            &SslClient::_onResolve,
            shared_from_this()));
}

void SslClient::_onResolve(beast::error_code ec,
                           asio::ip::tcp::resolver::results_type results)
{
    if(ec)
        return _processError(ec, "Resolve");

    auto& layer = beast::get_lowest_layer(_stream);
    layer.expires_after(_timeout);

    // Make the connection on the IP address we get from a lookup
    layer.async_connect(
        results,
        beast::bind_front_handler(
            &SslClient::_onConnect,
            shared_from_this()));
}

void SslClient::_onConnect(beast::error_code ec,
                           asio::ip::tcp::resolver::results_type::endpoint_type endpoint)
{
    boost::ignore_unused(endpoint);

    if(ec)
        return _processError(ec, "Connect");

    // Perform the SSL handshake
    _stream.async_handshake(
        asio::ssl::stream_base::client,
        beast::bind_front_handler(
            &SslClient::_onHandshake,
            shared_from_this()));
}

void SslClient::_onHandshake(beast::error_code ec)
{
    if(ec)
        return _processError(ec, "Handshake");

    auto& layer = beast::get_lowest_layer(_stream);
    layer.expires_after(_timeout);

    // Send the HTTP request to the remote host
    http::async_write(
        _stream, _request,
        beast::bind_front_handler(
            &SslClient::_onWrite,
            shared_from_this()));
}

void SslClient::_onWrite(beast::error_code ec,
                         size_t bytes_transferred)
{
    boost::ignore_unused(bytes_transferred);

    if(ec)
        return _processError(ec, "Write");

    auto& layer = beast::get_lowest_layer(_stream);
    layer.expires_after(_timeout);

    // Receive the HTTP response
    http::async_read(
        _stream, _buffer, _response,
        beast::bind_front_handler(
            &SslClient::_onRead,
            shared_from_this()));
}

void SslClient::_onRead(beast::error_code ec,
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

    auto& layer = beast::get_lowest_layer(_stream);
    layer.expires_after(_timeout);

    // Gracefully close the stream
    _stream.async_shutdown(
        beast::bind_front_handler(
            &SslClient::_onShutdown,
            shared_from_this()));
}

void SslClient::_onShutdown(beast::error_code ec)
{
    if(ec && ec != asio::ssl::error::stream_errors::stream_truncated)
        LOG(error) << "Shutdown: " << ec.message();
}

void SslClient::_processError(beast::error_code const& ec, std::string_view msg)
{
    std::this_thread::sleep_for(_errorTimeout);

    LOG(error) << msg << ": " << ec.message();
    _ec = ec;
    _condition.notify_all();
}

}
