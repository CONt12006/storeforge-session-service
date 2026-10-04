#include "session/session_store.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace storeforge::session {

namespace {

std::int64_t now_epoch() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string field_value(redisReply* reply, const std::string& field) {
    if (reply == nullptr || reply->type != REDIS_REPLY_ARRAY) {
        return {};
    }

    for (std::size_t i = 0; i + 1 < reply->elements; i += 2) {
        auto* key = reply->element[i];
        auto* value = reply->element[i + 1];
        if (key != nullptr && value != nullptr && std::string(key->str, key->len) == field) {
            return std::string(value->str, value->len);
        }
    }

    return {};
}

}

RedisSessionStore::RedisSessionStore(std::shared_ptr<RedisClient> redis) : redis_(std::move(redis)) {}

std::string RedisSessionStore::session_key(const std::string& session_id) {
    return "session:record:" + session_id;
}

std::string RedisSessionStore::user_sessions_key(std::int64_t user_id) {
    return "session:user:" + std::to_string(user_id);
}

SessionRecord RedisSessionStore::parse_session(const std::string& id, redisReply* reply) {
    SessionRecord record;
    record.id = id;
    record.user_id = std::stoll(field_value(reply, "user_id"));
    record.token_hash = field_value(reply, "token_hash");
    record.device_id = field_value(reply, "device_id");
    record.device_name = field_value(reply, "device_name");
    record.ip_address = field_value(reply, "ip_address");
    record.user_agent = field_value(reply, "user_agent");
    record.created_at = std::stoll(field_value(reply, "created_at"));
    record.last_seen_at = std::stoll(field_value(reply, "last_seen_at"));
    record.expires_at = std::stoll(field_value(reply, "expires_at"));
    return record;
}

void RedisSessionStore::create(const CreateSessionCommand& command, std::size_t max_sessions) {
    const auto now = now_epoch();
    const auto expires_at = now + command.ttl_seconds;

    static const std::string script = R"(
local session_key = KEYS[1]
local user_key = KEYS[2]
local session_id = ARGV[1]
local max_sessions = tonumber(ARGV[2])
local ttl = tonumber(ARGV[3])
redis.call('HSET', session_key,
    'user_id', ARGV[4],
    'token_hash', ARGV[5],
    'device_id', ARGV[6],
    'device_name', ARGV[7],
    'ip_address', ARGV[8],
    'user_agent', ARGV[9],
    'created_at', ARGV[10],
    'last_seen_at', ARGV[10],
    'expires_at', ARGV[11])
redis.call('EXPIRE', session_key, ttl)
redis.call('ZADD', user_key, ARGV[10], session_id)
redis.call('EXPIRE', user_key, ttl)
local count = redis.call('ZCARD', user_key)
while count > max_sessions do
    local oldest = redis.call('ZRANGE', user_key, 0, 0)[1]
    if not oldest then break end
    redis.call('ZREM', user_key, oldest)
    redis.call('DEL', 'session:record:' .. oldest)
    count = count - 1
end
return 1
)";

    redis_->command({
        "EVAL", script, "2",
        session_key(command.session_id),
        user_sessions_key(command.user_id),
        command.session_id,
        std::to_string(max_sessions),
        std::to_string(command.ttl_seconds),
        std::to_string(command.user_id),
        command.token_hash,
        command.device_id,
        command.device_name,
        command.ip_address,
        command.user_agent,
        std::to_string(now),
        std::to_string(expires_at)
    });
}

std::optional<SessionRecord> RedisSessionStore::find(const std::string& session_id) {
    auto reply = redis_->command({"HGETALL", session_key(session_id)});
    if (reply->type != REDIS_REPLY_ARRAY || reply->elements == 0) {
        return std::nullopt;
    }
    return parse_session(session_id, reply.get());
}

bool RedisSessionStore::rotate(const RotateSessionCommand& command) {
    const auto now = now_epoch();
    const auto expires_at = now + command.ttl_seconds;

    static const std::string script = R"(
local current = redis.call('HGET', KEYS[1], 'token_hash')
if not current or current ~= ARGV[1] then
    return 0
end
redis.call('HSET', KEYS[1],
    'token_hash', ARGV[2],
    'last_seen_at', ARGV[3],
    'expires_at', ARGV[4])
redis.call('EXPIRE', KEYS[1], ARGV[5])
local user_id = redis.call('HGET', KEYS[1], 'user_id')
if user_id then
    local user_key = 'session:user:' .. user_id
    redis.call('ZADD', user_key, ARGV[3], ARGV[6])
    redis.call('EXPIRE', user_key, ARGV[5])
end
return 1
)";

    auto reply = redis_->command({
        "EVAL", script, "1", session_key(command.session_id),
        command.current_token_hash,
        command.new_token_hash,
        std::to_string(now),
        std::to_string(expires_at),
        std::to_string(command.ttl_seconds),
        command.session_id
    });

    return reply->type == REDIS_REPLY_INTEGER && reply->integer == 1;
}

bool RedisSessionStore::revoke(const std::string& session_id, std::optional<std::int64_t> user_id) {
    static const std::string script = R"(
local stored_user_id = redis.call('HGET', KEYS[1], 'user_id')
if not stored_user_id then
    return 0
end
if ARGV[2] ~= '' and stored_user_id ~= ARGV[2] then
    return -1
end
redis.call('DEL', KEYS[1])
redis.call('ZREM', 'session:user:' .. stored_user_id, ARGV[1])
return 1
)";

    auto reply = redis_->command({
        "EVAL", script, "1", session_key(session_id),
        session_id,
        user_id.has_value() ? std::to_string(*user_id) : ""
    });

    return reply->type == REDIS_REPLY_INTEGER && reply->integer == 1;
}

std::size_t RedisSessionStore::revoke_all(std::int64_t user_id) {
    static const std::string script = R"(
local ids = redis.call('ZRANGE', KEYS[1], 0, -1)
for _, id in ipairs(ids) do
    redis.call('DEL', 'session:record:' .. id)
end
redis.call('DEL', KEYS[1])
return #ids
)";

    auto reply = redis_->command({"EVAL", script, "1", user_sessions_key(user_id)});
    return reply->type == REDIS_REPLY_INTEGER ? static_cast<std::size_t>(reply->integer) : 0;
}

std::vector<SessionRecord> RedisSessionStore::list(std::int64_t user_id) {
    auto ids_reply = redis_->command({"ZREVRANGE", user_sessions_key(user_id), "0", "-1"});
    std::vector<SessionRecord> records;

    if (ids_reply->type != REDIS_REPLY_ARRAY) {
        return records;
    }

    for (std::size_t i = 0; i < ids_reply->elements; ++i) {
        auto* item = ids_reply->element[i];
        if (item == nullptr) {
            continue;
        }
        const std::string id(item->str, item->len);
        auto record = find(id);
        if (record.has_value()) {
            records.push_back(std::move(*record));
        } else {
            redis_->command({"ZREM", user_sessions_key(user_id), id});
        }
    }

    return records;
}

bool RedisSessionStore::healthy() {
    return redis_->ping();
}

}
