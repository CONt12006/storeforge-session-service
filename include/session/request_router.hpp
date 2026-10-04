#pragma once

#include "session/session_service.hpp"

#include <boost/beast/http.hpp>
#include <boost/json/value.hpp>

#include <memory>
#include <string>

namespace storeforge::session {

namespace http = boost::beast::http;
using HttpRequest = http::request<http::string_body>;
using HttpResponse = http::response<http::string_body>;

class RequestRouter {
public:
    RequestRouter(std::shared_ptr<SessionService> service, std::string api_key);
    HttpResponse handle(const HttpRequest& request);

private:
    bool authorized(const HttpRequest& request) const;
    HttpResponse route(const HttpRequest& request);
    HttpResponse health();
    HttpResponse create_session(const HttpRequest& request);
    HttpResponse verify_session(const HttpRequest& request, const std::string& session_id);
    HttpResponse rotate_session(const HttpRequest& request, const std::string& session_id);
    HttpResponse revoke_session(const HttpRequest& request, const std::string& session_id);
    HttpResponse list_sessions(std::int64_t user_id);
    HttpResponse revoke_all(std::int64_t user_id);

    std::shared_ptr<SessionService> service_;
    std::string api_key_;
};

class ResponseFactory {
public:
    static HttpResponse json(http::status status, const boost::json::value& body, unsigned version = 11);
    static HttpResponse error(http::status status, const std::string& message, unsigned version = 11);
};

}
