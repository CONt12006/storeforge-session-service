#pragma once

#include "session/models.hpp"
#include "session/redis_client.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace storeforge::session {

class ISessionStore {
public:
    virtual ~ISessionStore() = default;
    virtual void create(const CreateSessionCommand& command, std::size_t max_sessions) = 0;
    virtual std::optional<SessionRecord> find(const std::string& session_id) = 0;
    virtual bool rotate(const RotateSessionCommand& command) = 0;
    virtual bool revoke(const std::string& session_id, std::optional<std::int64_t> user_id) = 0;
    virtual std::size_t revoke_all(std::int64_t user_id) = 0;
    virtual std::vector<SessionRecord> list(std::int64_t user_id) = 0;
    virtual bool healthy() = 0;
};

class RedisSessionStore final : public ISessionStore {
public:
    explicit RedisSessionStore(std::shared_ptr<RedisClient> redis);

    void create(const CreateSessionCommand& command, std::size_t max_sessions) override;
    std::optional<SessionRecord> find(const std::string& session_id) override;
    bool rotate(const RotateSessionCommand& command) override;
    bool revoke(const std::string& session_id, std::optional<std::int64_t> user_id) override;
    std::size_t revoke_all(std::int64_t user_id) override;
    std::vector<SessionRecord> list(std::int64_t user_id) override;
    bool healthy() override;

private:
    static std::string session_key(const std::string& session_id);
    static std::string user_sessions_key(std::int64_t user_id);
    static SessionRecord parse_session(const std::string& id, redisReply* reply);

    std::shared_ptr<RedisClient> redis_;
};

}
