#pragma once

#include <memory>

#include "client.hpp"
#include "ssl_client.hpp"

#include "server.hpp"

namespace http
{

namespace asio = boost::asio;

class Factory
    : std::enable_shared_from_this<Factory>
{
public:
    static std::shared_ptr<Factory> getFactory();

    Factory();
    virtual ~Factory();

    bool addCertificate(std::string_view cert);

    std::shared_ptr<Client>     getClient(std::string_view host = "127.0.0.1", uint16_t port = 80);
    std::shared_ptr<SslClient>  getSslClient(std::string_view host = "127.0.0.1", uint16_t port = 443);

    std::shared_ptr<Server>     getServer(std::string_view host = "0.0.0.0", uint16_t port = 7500);

private:
    asio::io_context            _ioc;
    asio::io_context::work      _work;
    asio::ssl::context          _ctx;
    std::vector<std::thread>    _threads;
};

}
