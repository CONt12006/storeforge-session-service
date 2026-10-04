#include "session/config.hpp"

#include <cstdlib>
#include <stdexcept>

namespace storeforge::session {

namespace {

std::string env_string(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return value == nullptr ? fallback : std::string(value);
}

int env_int(const char* name, int fallback) {
    const char* value = std::getenv(name);
    return value == nullptr ? fallback : std::stoi(value);
}

}

Config Config::from_environment() {
    Config config;
    config.host_ = env_string("SESSION_HOST", "0.0.0.0");
    config.port_ = static_cast<std::uint16_t>(env_int("SESSION_PORT", 8081));
    config.redis_host_ = env_string("REDIS_HOST", "redis");
    config.redis_port_ = env_int("REDIS_PORT", 6379);
    config.redis_database_ = env_int("REDIS_DATABASE", 1);
    config.redis_password_ = env_string("REDIS_PASSWORD", "");
    config.api_key_ = env_string("SESSION_API_KEY", "local-session-service-key");
    config.max_sessions_per_user_ = static_cast<std::size_t>(env_int("MAX_SESSIONS_PER_USER", 5));

    if (config.api_key_.empty()) {
        throw std::runtime_error("SESSION_API_KEY must not be empty");
    }

    if (config.max_sessions_per_user_ == 0) {
        throw std::runtime_error("MAX_SESSIONS_PER_USER must be positive");
    }

    return config;
}

const std::string& Config::host() const { return host_; }
std::uint16_t Config::port() const { return port_; }
const std::string& Config::redis_host() const { return redis_host_; }
int Config::redis_port() const { return redis_port_; }
int Config::redis_database() const { return redis_database_; }
const std::string& Config::redis_password() const { return redis_password_; }
const std::string& Config::api_key() const { return api_key_; }
std::size_t Config::max_sessions_per_user() const { return max_sessions_per_user_; }

}
