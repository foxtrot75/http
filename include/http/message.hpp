#pragma once

#include <map>
#include <string>

#include <boost/beast.hpp>

namespace http
{

namespace http = boost::beast::http;

struct Request
{
    std::string                         target;
    std::map<std::string, std::string>  params;
    std::map<std::string, std::string>  fields;
    std::string                         body;
};

struct Response
{
    http::status                        result = http::status::ok;
    std::map<std::string, std::string>  fields;
    std::string                         body;
};

}
