# StoreForge Session Service

Standalone C++20 microservice for centralized session management.

The service stores active sessions in Redis and exposes an internal HTTP API for an external authentication service. It manages device metadata, IP addresses, User-Agent, refresh-token hashes, refresh rotation, single-session revoke, logout everywhere and a per-user active-session limit.

## Stack

- C++20
- Boost.Beast
- Boost.Asio
- Boost.JSON
- hiredis
- Redis
- CMake
- Docker

## Run

```bash
cp .env.example .env
docker compose up --build
```

Service:

```text
http://localhost:8081
```

Health check:

```bash
curl http://localhost:8081/health
```

Protected endpoints require:

```text
X-Internal-Api-Key: <SESSION_API_KEY>
```

## Configuration

| Variable | Default | Description |
|---|---:|---|
| `SESSION_HOST` | `0.0.0.0` | HTTP bind address |
| `SESSION_PORT` | `8081` | HTTP port |
| `SESSION_API_KEY` | `local-session-service-key` | Internal API key |
| `MAX_SESSIONS_PER_USER` | `5` | Maximum active sessions per user |
| `REDIS_HOST` | `redis` | Redis host |
| `REDIS_PORT` | `6379` | Redis port |
| `REDIS_DATABASE` | `1` | Redis database |
| `REDIS_PASSWORD` | empty | Redis password |

## API

### Create session

`POST /v1/sessions`

```json
{
  "session_id": "2e070e3a-6e95-45e5-9c0e-d28897f60d1c",
  "user_id": 42,
  "token_hash": "sha256-hex-hash",
  "device_id": "chrome-macbook",
  "device_name": "Chrome on MacBook",
  "ip_address": "203.0.113.10",
  "user_agent": "Mozilla/5.0",
  "ttl_seconds": 2592000
}
```

### Verify refresh token hash

`POST /v1/sessions/{session_id}/verify`

```json
{
  "token_hash": "sha256-hex-hash"
}
```

### Rotate refresh token hash

`POST /v1/sessions/{session_id}/rotate`

```json
{
  "current_token_hash": "old-sha256-hex-hash",
  "new_token_hash": "new-sha256-hex-hash",
  "ttl_seconds": 2592000
}
```

### Revoke one session

`DELETE /v1/sessions/{session_id}`

An optional body can restrict the revoke to a specific user:

```json
{
  "user_id": 42
}
```

### List active sessions

`GET /v1/users/{user_id}/sessions`

### Revoke all user sessions

`DELETE /v1/users/{user_id}/sessions`

### Health

`GET /health`

`GET /healthz`

## Example

```bash
curl -X POST http://localhost:8081/v1/sessions \
  -H 'Content-Type: application/json' \
  -H 'X-Internal-Api-Key: change-me' \
  -d '{
    "session_id":"2e070e3a-6e95-45e5-9c0e-d28897f60d1c",
    "user_id":42,
    "token_hash":"abc123",
    "device_id":"chrome-win",
    "device_name":"Chrome on Windows",
    "ip_address":"127.0.0.1",
    "user_agent":"curl",
    "ttl_seconds":2592000
  }'
```

## Local build

Dependencies on Debian/Ubuntu:

```bash
sudo apt-get install build-essential cmake pkg-config libboost-all-dev libhiredis-dev
```

Build and test:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Run against an available Redis instance:

```bash
SESSION_API_KEY=change-me \
REDIS_HOST=127.0.0.1 \
REDIS_PORT=6379 \
REDIS_DATABASE=0 \
./build/session-service
```

## Structure

```text
storeforge-session-service/
├── include/session/
├── src/
├── tests/
├── CMakeLists.txt
├── Dockerfile
├── docker-compose.yml
├── .env.example
└── README.md
```
