#include "session/session_service.hpp"

#include <stdexcept>

namespace storeforge::session {

SessionService::SessionService(std::shared_ptr<ISessionStore> store, std::size_t max_sessions_per_user)
    : store_(std::move(store)), max_sessions_per_user_(max_sessions_per_user) {}

void SessionService::validate_hash(const std::string& value) {
    if (value.size() != 64) {
        throw std::invalid_argument("token hash must contain 64 hexadecimal characters");
    }

    for (char ch : value) {
        const bool digit = ch >= '0' && ch <= '9';
        const bool lower = ch >= 'a' && ch <= 'f';
        const bool upper = ch >= 'A' && ch <= 'F';
        if (!digit && !lower && !upper) {
            throw std::invalid_argument("token hash must contain 64 hexadecimal characters");
        }
    }
}

void SessionService::validate_create(const CreateSessionCommand& command) {
    if (command.session_id.empty()) {
        throw std::invalid_argument("session_id is required");
    }
    if (command.user_id <= 0) {
        throw std::invalid_argument("user_id must be positive");
    }
    if (command.ttl_seconds <= 0) {
        throw std::invalid_argument("ttl_seconds must be positive");
    }
    validate_hash(command.token_hash);
}

SessionRecord SessionService::create(const CreateSessionCommand& command) {
    validate_create(command);
    store_->create(command, max_sessions_per_user_);
    auto session = store_->find(command.session_id);
    if (!session.has_value()) {
        throw std::runtime_error("created session cannot be loaded");
    }
    return *session;
}

VerificationResult SessionService::verify(const std::string& session_id, const std::string& token_hash) {
    validate_hash(token_hash);
    auto session = store_->find(session_id);
    if (!session.has_value() || session->token_hash != token_hash) {
        return {false, std::nullopt};
    }
    return {true, session};
}

bool SessionService::rotate(const RotateSessionCommand& command) {
    validate_hash(command.current_token_hash);
    validate_hash(command.new_token_hash);
    if (command.ttl_seconds <= 0) {
        throw std::invalid_argument("ttl_seconds must be positive");
    }
    return store_->rotate(command);
}

bool SessionService::revoke(const std::string& session_id, std::optional<std::int64_t> user_id) {
    return store_->revoke(session_id, user_id);
}

std::size_t SessionService::revoke_all(std::int64_t user_id) {
    if (user_id <= 0) {
        throw std::invalid_argument("user_id must be positive");
    }
    return store_->revoke_all(user_id);
}

std::vector<SessionRecord> SessionService::list(std::int64_t user_id) {
    if (user_id <= 0) {
        throw std::invalid_argument("user_id must be positive");
    }
    return store_->list(user_id);
}

bool SessionService::healthy() {
    return store_->healthy();
}

}
