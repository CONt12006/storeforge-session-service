#include "session/request_router.hpp"

#include <boost/json.hpp>

#include <charconv>
#include <stdexcept>
#include <string_view>

namespace storeforge::session {

namespace {

boost::json::object session_json(const SessionRecord& record) {
    boost::json::object object;
    object["id"] = record.id;
    object["user_id"] = record.user_id;
    object["device_id"] = record.device_id;
    object["device_name"] = record.device_name;
    object["ip_address"] = record.ip_address;
    object["user_agent"] = record.user_agent;
    object["created_at"] = record.created_at;
    object["last_seen_at"] = record.last_seen_at;
    object["expires_at"] = record.expires_at;
    return object;
}

std::string require_string(const boost::json::object& object, const char* key) {
    auto* value = object.if_contains(key);
    if (value == nullptr || !value->is_string()) {
        throw std::invalid_argument(std::string(key) + " is required");
    }
    return std::string(value->as_string().c_str());
}

std::string optional_string(const boost::json::object& object, const char* key) {
    auto* value = object.if_contains(key);
    if (value == nullptr || value->is_null()) {
        return {};
    }
    if (!value->is_string()) {
        throw std::invalid_argument(std::string(key) + " must be a string");
    }
    return std::string(value->as_string().c_str());
}

std::int64_t require_int(const boost::json::object& object, const char* key) {
    auto* value = object.if_contains(key);
    if (value == nullptr || !value->is_int64()) {
        throw std::invalid_argument(std::string(key) + " is required");
    }
    return value->as_int64();
}

boost::json::object parse_object(const std::string& body) {
    auto value = boost::json::parse(body);
    if (!value.is_object()) {
        throw std::invalid_argument("request body must be a JSON object");
    }
    return value.as_object();
}

bool parse_user_path(std::string_view target, std::int64_t& user_id, bool& sessions_suffix) {
    constexpr std::string_view prefix = "/v1/users/";
    if (!target.starts_with(prefix)) {
        return false;
    }

    target.remove_prefix(prefix.size());
    auto slash = target.find('/');
    auto id_part = slash == std::string_view::npos ? target : target.substr(0, slash);
    auto [ptr, error] = std::from_chars(id_part.data(), id_part.data() + id_part.size(), user_id);
    if (error != std::errc() || ptr != id_part.data() + id_part.size()) {
        return false;
    }

    sessions_suffix = slash != std::string_view::npos && target.substr(slash) == "/sessions";
    return sessions_suffix;
}

bool parse_session_path(std::string_view target, std::string& session_id, std::string& action) {
    constexpr std::string_view prefix = "/v1/sessions/";
    if (!target.starts_with(prefix)) {
        return false;
    }

    target.remove_prefix(prefix.size());
    auto slash = target.find('/');
    if (slash == std::string_view::npos) {
        session_id = std::string(target);
        action.clear();
        return !session_id.empty();
    }

    session_id = std::string(target.substr(0, slash));
    action = std::string(target.substr(slash + 1));
    return !session_id.empty();
}

}

RequestRouter::RequestRouter(std::shared_ptr<SessionService> service, std::string api_key)
    : service_(std::move(service)), api_key_(std::move(api_key)) {}

bool RequestRouter::authorized(const HttpRequest& request) const {
    auto value = request.base()["X-Internal-Api-Key"];
    return !value.empty() && value == api_key_;
}

HttpResponse RequestRouter::handle(const HttpRequest& request) {
    try {
        if (request.target() == "/health" || request.target() == "/healthz") {
            return health();
        }
        if (!authorized(request)) {
            return ResponseFactory::error(http::status::unauthorized, "invalid internal API key", request.version());
        }
        auto response = route(request);
        response.version(request.version());
        return response;
    } catch (const boost::json::system_error& error) {
        return ResponseFactory::error(http::status::bad_request, error.what(), request.version());
    } catch (const std::invalid_argument& error) {
        return ResponseFactory::error(http::status::bad_request, error.what(), request.version());
    } catch (const std::exception& error) {
        return ResponseFactory::error(http::status::internal_server_error, error.what(), request.version());
    }
}

HttpResponse RequestRouter::route(const HttpRequest& request) {
    const std::string target(request.target());

    if (request.method() == http::verb::post && target == "/v1/sessions") {
        return create_session(request);
    }

    std::string session_id;
    std::string action;
    if (parse_session_path(target, session_id, action)) {
        if (request.method() == http::verb::post && action == "verify") {
            return verify_session(request, session_id);
        }
        if (request.method() == http::verb::post && action == "rotate") {
            return rotate_session(request, session_id);
        }
        if (request.method() == http::verb::delete_ && action.empty()) {
            return revoke_session(request, session_id);
        }
    }

    std::int64_t user_id{};
    bool sessions_suffix{};
    if (parse_user_path(target, user_id, sessions_suffix) && sessions_suffix) {
        if (request.method() == http::verb::get) {
            return list_sessions(user_id);
        }
        if (request.method() == http::verb::delete_) {
            return revoke_all(user_id);
        }
    }

    return ResponseFactory::error(http::status::not_found, "route not found", request.version());
}

HttpResponse RequestRouter::health() {
    boost::json::object body;
    const bool ok = service_->healthy();
    body["status"] = ok ? "ok" : "unavailable";
    return ResponseFactory::json(ok ? http::status::ok : http::status::service_unavailable, body);
}

HttpResponse RequestRouter::create_session(const HttpRequest& request) {
    const auto body = parse_object(request.body());
    CreateSessionCommand command;
    command.session_id = require_string(body, "session_id");
    command.user_id = require_int(body, "user_id");
    command.token_hash = require_string(body, "token_hash");
    command.device_id = optional_string(body, "device_id");
    command.device_name = optional_string(body, "device_name");
    command.ip_address = optional_string(body, "ip_address");
    command.user_agent = optional_string(body, "user_agent");
    command.ttl_seconds = require_int(body, "ttl_seconds");

    auto created = service_->create(command);
    return ResponseFactory::json(http::status::created, session_json(created), request.version());
}

HttpResponse RequestRouter::verify_session(const HttpRequest& request, const std::string& session_id) {
    const auto body = parse_object(request.body());
    auto result = service_->verify(session_id, require_string(body, "token_hash"));
    boost::json::object response;
    response["valid"] = result.valid;
    if (result.session.has_value()) {
        response["session"] = session_json(*result.session);
    }
    return ResponseFactory::json(http::status::ok, response, request.version());
}

HttpResponse RequestRouter::rotate_session(const HttpRequest& request, const std::string& session_id) {
    const auto body = parse_object(request.body());
    RotateSessionCommand command;
    command.session_id = session_id;
    command.current_token_hash = require_string(body, "current_token_hash");
    command.new_token_hash = require_string(body, "new_token_hash");
    command.ttl_seconds = require_int(body, "ttl_seconds");

    const bool rotated = service_->rotate(command);
    boost::json::object response;
    response["rotated"] = rotated;
    return ResponseFactory::json(rotated ? http::status::ok : http::status::unauthorized, response, request.version());
}

HttpResponse RequestRouter::revoke_session(const HttpRequest& request, const std::string& session_id) {
    std::optional<std::int64_t> user_id;
    if (!request.body().empty()) {
        const auto body = parse_object(request.body());
        if (auto* value = body.if_contains("user_id"); value != nullptr && value->is_int64()) {
            user_id = value->as_int64();
        }
    }
    const bool revoked = service_->revoke(session_id, user_id);
    boost::json::object response;
    response["revoked"] = revoked;
    return ResponseFactory::json(http::status::ok, response, request.version());
}

HttpResponse RequestRouter::list_sessions(std::int64_t user_id) {
    auto sessions = service_->list(user_id);
    boost::json::array items;
    for (const auto& session : sessions) {
        items.push_back(session_json(session));
    }
    boost::json::object response;
    response["sessions"] = std::move(items);
    return ResponseFactory::json(http::status::ok, response);
}

HttpResponse RequestRouter::revoke_all(std::int64_t user_id) {
    boost::json::object response;
    response["revoked"] = static_cast<std::int64_t>(service_->revoke_all(user_id));
    return ResponseFactory::json(http::status::ok, response);
}

HttpResponse ResponseFactory::json(http::status status, const boost::json::value& body, unsigned version) {
    HttpResponse response{status, version};
    response.set(http::field::content_type, "application/json");
    response.set(http::field::server, "storeforge-session-service");
    response.keep_alive(false);
    response.body() = boost::json::serialize(body);
    response.prepare_payload();
    return response;
}

HttpResponse ResponseFactory::error(http::status status, const std::string& message, unsigned version) {
    boost::json::object body;
    body["detail"] = message;
    return json(status, body, version);
}

}
