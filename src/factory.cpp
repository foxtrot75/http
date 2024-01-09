#include <string>

#include <boost/asio.hpp>
#include <boost/beast/ssl.hpp>

#include "http/factory.hpp"

namespace http
{

std::shared_ptr<Factory> Factory::getFactory()
{
    static std::shared_ptr<Factory> _factory = std::make_shared<Factory>();
    return _factory;
}

Factory::Factory()
    : _work(_ioc)
    , _ctx(boost::asio::ssl::context::tlsv12_client)
{
    _ctx.set_default_verify_paths();
    _ctx.set_verify_mode(boost::asio::ssl::verify_peer);

    for(int i = 0; i < 2; ++i)
        _threads.emplace_back(std::thread([this]() { _ioc.run(); }));
}

Factory::~Factory()
{
    _ioc.stop();

    for(auto& t: _threads)
        t.join();
}

bool Factory::addCertificate(std::string_view cert)
{
    if(cert.empty())
        return true;

    boost::system::error_code ec;
    _ctx.add_certificate_authority({cert.data(), cert.size()}, ec);

    if(ec)
        return false;

    return true;
}

std::shared_ptr<Client> Factory::getClient(std::string_view host, uint16_t port)
{
    auto client = std::make_shared<Client>(_ioc);
    client->setup(host, port);
    return client;
}

std::shared_ptr<SslClient> Factory::getSslClient(std::string_view host, uint16_t port)
{
    auto client = std::make_shared<SslClient>(_ioc, _ctx);
    client->setup(host, port);
    return client;
}

std::shared_ptr<Server> Factory::getServer(std::string_view host, uint16_t port)
{
    auto server = std::make_shared<Server>(_ioc);
    server->setup(host, port);
    return server;
}

}
