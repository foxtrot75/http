#pragma once

#include <map>
#include <string>

#include <boost/beast.hpp>

namespace Http
{

namespace http = boost::beast::http;

struct Query
{
    std::string target;
    std::map<std::string, std::string> params;
    std::map<http::field, std::string> fields;
    std::string body;
};

}
