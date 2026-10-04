#pragma once

#include <cstdint>
#include <string>

namespace storeforge::session {

class Config {
public:
    static Config from_environment();

    const std::string& host() const;
    std::uint16_t port() const;
    const std::string& redis_host() const;
    int redis_port() const;
    int redis_database() const;
    const std::string& redis_password() const;
    const std::string& api_key() const;
    std::size_t max_sessions_per_user() const;

private:
    std::string host_;
    std::uint16_t port_{};
    std::string redis_host_;
    int redis_port_{};
    int redis_database_{};
    std::string redis_password_;
    std::string api_key_;
    std::size_t max_sessions_per_user_{};
};

}
