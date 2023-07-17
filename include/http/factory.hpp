#pragma once

#include <memory>

#include "client.hpp"
#include "ssl_client.hpp"

namespace Http
{

namespace asio = boost::asio;

class Factory
{
public:
    explicit Factory();
    explicit Factory(std::string const& host);
    virtual ~Factory();

    void setHost(std::string const& host);

    std::shared_ptr<Client>     getClient();
    std::shared_ptr<SslClient>  getSslClient();

private:
    asio::io_context                _ioc;
    asio::io_context::work          _work;
    asio::ssl::context              _ctx;
    std::shared_ptr<std::thread>    _thread;
    std::string                     _host;
};

}
