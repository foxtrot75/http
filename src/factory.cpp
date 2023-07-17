#include <string>

#include <boost/asio.hpp>
#include <boost/beast/ssl.hpp>

#include "http/factory.hpp"

namespace Http
{

Factory::Factory()
    : Factory("")
{}

Factory::Factory(std::string const& host)
    : _work(_ioc)
    , _ctx(boost::asio::ssl::context::tlsv12_client)
    , _host(host)
{
    _ctx.set_default_verify_paths();
    _ctx.set_verify_mode(boost::asio::ssl::verify_none);

    _thread = std::make_shared<std::thread>([this]() { _ioc.run(); });
}

Factory::~Factory()
{
    _ioc.stop();
    _thread->join();
}

void Factory::setHost(std::string const& host)
{
    _host = host;
}

std::shared_ptr<Client> Factory::getClient()
{
    auto client = std::make_shared<Client>(_ioc);
    client->setup(_host, 80);
    return client;
}

std::shared_ptr<SslClient> Factory::getSslClient()
{
    auto client = std::make_shared<SslClient>(_ioc, _ctx);
    client->setup(_host, 443);
    return client;
}

}
