#pragma once

#include "session/models.hpp"
#include "session/session_store.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace storeforge::session {

class SessionService {
public:
    SessionService(std::shared_ptr<ISessionStore> store, std::size_t max_sessions_per_user);

    SessionRecord create(const CreateSessionCommand& command);
    VerificationResult verify(const std::string& session_id, const std::string& token_hash);
    bool rotate(const RotateSessionCommand& command);
    bool revoke(const std::string& session_id, std::optional<std::int64_t> user_id = std::nullopt);
    std::size_t revoke_all(std::int64_t user_id);
    std::vector<SessionRecord> list(std::int64_t user_id);
    bool healthy();

private:
    static void validate_create(const CreateSessionCommand& command);
    static void validate_hash(const std::string& value);

    std::shared_ptr<ISessionStore> store_;
    std::size_t max_sessions_per_user_;
};

}
