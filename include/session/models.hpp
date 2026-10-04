#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace storeforge::session {

struct SessionRecord {
    std::string id;
    std::int64_t user_id{};
    std::string token_hash;
    std::string device_id;
    std::string device_name;
    std::string ip_address;
    std::string user_agent;
    std::int64_t created_at{};
    std::int64_t last_seen_at{};
    std::int64_t expires_at{};
};

struct CreateSessionCommand {
    std::string session_id;
    std::int64_t user_id{};
    std::string token_hash;
    std::string device_id;
    std::string device_name;
    std::string ip_address;
    std::string user_agent;
    std::int64_t ttl_seconds{};
};

struct RotateSessionCommand {
    std::string session_id;
    std::string current_token_hash;
    std::string new_token_hash;
    std::int64_t ttl_seconds{};
};

struct VerificationResult {
    bool valid{};
    std::optional<SessionRecord> session;
};

}
