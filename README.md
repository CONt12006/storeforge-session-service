# StoreForge Session Service

> Микросервис централизованного управления пользовательскими сессиями для платформы **StoreForge**.

[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus\&logoColor=white)](https://isocpp.org)
[![Boost](https://img.shields.io/badge/Boost-Beast%20%2F%20Asio-5A69A6?logo=boost\&logoColor=white)](https://www.boost.org)
[![Redis](https://img.shields.io/badge/Redis-Session%20Storage-DC382D?logo=redis\&logoColor=white)](https://redis.io)
[![CMake](https://img.shields.io/badge/CMake-Build-064F8C?logo=cmake\&logoColor=white)](https://cmake.org)
[![Docker](https://img.shields.io/badge/Docker-Ready-2496ED?logo=docker\&logoColor=white)](https://docker.com)

---

## Overview

**StoreForge Session Service** — отдельный микросервис на C++20 для централизованного управления пользовательскими сессиями.

Сервис предназначен для работы совместно с внешним Auth Service. Auth Service отвечает за регистрацию, аутентификацию и выпуск JWT, а Session Service управляет жизненным циклом refresh-сессий.

Session Service хранит активные сессии в Redis, отслеживает устройства, IP-адреса и User-Agent, выполняет ротацию refresh-токенов, отзыв отдельных сессий, выход со всех устройств и ограничение количества активных сессий пользователя.

Сервис предоставляет внутренний HTTP API и не зависит от Python-кода Auth Service.

---

## Features

* создание пользовательских сессий;
* хранение активных сессий в Redis;
* привязка сессии к пользователю;
* хранение IP-адреса;
* хранение User-Agent;
* хранение идентификатора и названия устройства;
* SHA-256 hash refresh-токена вместо исходного токена;
* проверка refresh-токена;
* refresh-token rotation;
* отзыв конкретной сессии;
* logout everywhere;
* получение списка активных устройств пользователя;
* ограничение количества активных сессий пользователя;
* автоматическое удаление старых сессий при превышении лимита;
* TTL для session records;
* атомарные операции через Redis Lua scripts;
* внутренний API key для межсервисного взаимодействия;
* health-check endpoints;
* конфигурация через environment variables;
* Docker и Docker Compose;
* unit-тесты через CTest.

---

## Architecture

```text
Client
  │
  ▼
Auth Service
  │
  │ login / refresh / logout
  │
  ▼
StoreForge Session Service
  │
  ├── HTTP API
  │
  ├── SessionService
  │
  ├── SessionStore
  │
  └── RedisClient
          │
          ▼
        Redis
```

Auth Service генерирует access и refresh tokens, после чего передаёт данные refresh-сессии в Session Service.

Session Service не хранит пароль пользователя и не выпускает JWT. Его задача — управление состоянием активных пользовательских сессий.

---

## Layers

**HTTP Server**

Принимает HTTP-соединения и передаёт запросы в router.

**Request Router**

Маршрутизирует HTTP-запросы, проверяет `X-Internal-Api-Key`, валидирует входные данные и формирует HTTP-ответы.

**Session Service**

Содержит бизнес-логику создания, проверки, ротации и отзыва пользовательских сессий.

**Session Store**

Инкапсулирует операции хранения сессий и пользовательских индексов.

**Redis Client**

Отвечает за соединение с Redis и выполнение Redis-команд и Lua scripts.

---

## Session Lifecycle

После успешной аутентификации Auth Service создаёт refresh token и передаёт данные новой сессии в Session Service.

```text
Login
  │
  ▼
Generate Refresh Token
  │
  ▼
Calculate Token Hash
  │
  ▼
Create Session
  │
  ▼
Store Session in Redis
```

Session Service получает:

```text
session_id
user_id
token_hash
device_id
device_name
ip_address
user_agent
ttl_seconds
```

В Redis сохраняется только hash refresh-токена.

---

## Refresh Token Rotation

При обновлении token pair Auth Service сначала проверяет текущий refresh token через Session Service.

```text
Refresh Token
      │
      ▼
Calculate SHA-256 Hash
      │
      ▼
Verify Session
      │
      ▼
Generate New Refresh Token
      │
      ▼
Rotate Token Hash
      │
      ▼
Old Refresh Token Becomes Invalid
```

`session_id` при ротации не меняется. Благодаря этому одно физическое устройство продолжает отображаться как одна активная сессия.

---

## Active Devices

Каждая сессия содержит метаданные устройства:

```json
{
  "session_id": "2e070e3a-6e95-45e5-9c0e-d28897f60d1c",
  "user_id": 42,
  "device_id": "chrome-windows",
  "device_name": "Chrome on Windows",
  "ip_address": "203.0.113.10",
  "user_agent": "Mozilla/5.0",
  "created_at": 1780617600,
  "last_seen_at": 1780617600
}
```

Это позволяет Auth Service реализовать страницу управления активными устройствами пользователя.

---

## Session Limit

Количество активных сессий пользователя ограничивается параметром:

```text
MAX_SESSIONS_PER_USER
```

По умолчанию:

```text
5
```

Если после создания новой сессии лимит превышен, сервис отзывает самые старые активные сессии пользователя.

```text
User Sessions
     │
     ▼
Check Session Count
     │
     ├── count <= limit ──► keep sessions
     │
     └── count > limit
              │
              ▼
       Revoke Oldest Sessions
```

---

## Redis Storage

Основная запись сессии хранится отдельно от пользовательского индекса.

Пример ключей:

```text
session:record:<session_id>
session:user:<user_id>
```

`session:record:<session_id>` содержит данные конкретной сессии.

`session:user:<user_id>` хранит индекс активных сессий пользователя и используется для получения списка устройств и реализации session limit.

Ключи имеют TTL и автоматически удаляются после истечения срока жизни refresh-сессии.

---

## Internal Authentication

Все внутренние endpoints, кроме health-check, защищены API key.

Ключ передаётся через header:

```http
X-Internal-Api-Key: <SESSION_API_KEY>
```

Session Service предполагается размещать во внутренней сети и не публиковать напрямую наружу.

---

## API

| Method   | Endpoint                         | Description                         |
| -------- | -------------------------------- | ----------------------------------- |
| `POST`   | `/v1/sessions`                   | Создание новой сессии               |
| `POST`   | `/v1/sessions/{id}/verify`       | Проверка refresh-токена             |
| `POST`   | `/v1/sessions/{id}/rotate`       | Ротация refresh-токена              |
| `DELETE` | `/v1/sessions/{id}`              | Отзыв конкретной сессии             |
| `GET`    | `/v1/users/{user_id}/sessions`   | Получение активных сессий           |
| `DELETE` | `/v1/users/{user_id}/sessions`   | Выход пользователя со всех устройств |
| `GET`    | `/health`                        | Health check                        |
| `GET`    | `/healthz`                       | Health check                        |

---

## Create Session

```http
POST /v1/sessions
```

```json
{
  "session_id": "2e070e3a-6e95-45e5-9c0e-d28897f60d1c",
  "user_id": 42,
  "token_hash": "sha256-hex-hash",
  "device_id": "chrome-windows",
  "device_name": "Chrome on Windows",
  "ip_address": "203.0.113.10",
  "user_agent": "Mozilla/5.0",
  "ttl_seconds": 2592000
}
```

---

## Verify Session

```http
POST /v1/sessions/{session_id}/verify
```

```json
{
  "token_hash": "sha256-hex-hash"
}
```

Сервис проверяет существование сессии и совпадение hash refresh-токена.

---

## Rotate Session

```http
POST /v1/sessions/{session_id}/rotate
```

```json
{
  "current_token_hash": "old-sha256-hex-hash",
  "new_token_hash": "new-sha256-hex-hash",
  "ttl_seconds": 2592000
}
```

Операция выполняется атомарно. Если `current_token_hash` не совпадает с текущим значением, ротация не выполняется.

---

## Revoke Session

```http
DELETE /v1/sessions/{session_id}
```

При необходимости запрос можно ограничить конкретным пользователем:

```json
{
  "user_id": 42
}
```

---

## Logout Everywhere

```http
DELETE /v1/users/{user_id}/sessions
```

Удаляет все активные сессии пользователя.

---

## Tech Stack

| Technology  | Purpose                  |
| ----------- | ------------------------ |
| C++20       | Backend                  |
| Boost.Beast | HTTP server              |
| Boost.Asio  | Networking / I/O         |
| Boost.JSON  | JSON parsing             |
| hiredis     | Redis client             |
| Redis       | Session storage          |
| CMake       | Build system             |
| CTest       | Testing                  |
| Docker      | Containerization         |
| Docker Compose | Local environment    |

---

## Project Structure

```text
storeforge-session-service/
├── include/
│   └── session/
│       ├── config.hpp
│       ├── http_server.hpp
│       ├── models.hpp
│       ├── redis_client.hpp
│       ├── request_router.hpp
│       ├── session_service.hpp
│       └── session_store.hpp
├── src/
│   ├── boost_json.cpp
│   ├── config.cpp
│   ├── http_server.cpp
│   ├── main.cpp
│   ├── redis_client.cpp
│   ├── request_router.cpp
│   ├── session_service.cpp
│   └── session_store.cpp
├── tests/
│   └── session_service_tests.cpp
├── CMakeLists.txt
├── Dockerfile
├── docker-compose.yml
├── .env.example
└── README.md
```

---

## Running Locally

### Docker Compose

Создай `.env` из примера:

```bash
cp .env.example .env
```

Запусти сервис и Redis:

```bash
docker compose up --build
```

После запуска:

```text
Session Service: http://localhost:8081
Redis:           localhost:6379
```

Health check:

```bash
curl http://localhost:8081/health
```

---

## Local Build

Для Debian / Ubuntu установи зависимости:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config libboost-all-dev libhiredis-dev
```

Сборка:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Запуск тестов:

```bash
ctest --test-dir build --output-on-failure
```

Запуск сервиса с локальным Redis:

```bash
SESSION_API_KEY=change-me \
REDIS_HOST=127.0.0.1 \
REDIS_PORT=6379 \
REDIS_DATABASE=0 \
./build/session-service
```

---

## Configuration

| Variable                | Default                     | Description                         |
| ----------------------- | --------------------------- | ----------------------------------- |
| `SESSION_HOST`          | `0.0.0.0`                   | Адрес HTTP server                   |
| `SESSION_PORT`          | `8081`                      | HTTP port                           |
| `SESSION_API_KEY`       | `local-session-service-key` | Внутренний API key                  |
| `MAX_SESSIONS_PER_USER` | `5`                         | Максимум активных сессий            |
| `REDIS_HOST`            | `redis`                     | Redis host                          |
| `REDIS_PORT`            | `6379`                      | Redis port                          |
| `REDIS_DATABASE`        | `0`                         | Redis database                      |
| `REDIS_PASSWORD`        | empty                       | Redis password                      |

Для production необходимо заменить `SESSION_API_KEY` на случайное секретное значение и не хранить его непосредственно в репозитории.

---

## Integration with Auth Service

Auth Service и Session Service являются отдельными микросервисами.

Пример взаимодействия:

```text
Client
  │
  ▼
Auth Service
  │
  ├── PostgreSQL
  │
  ├── JWT generation
  │
  └── HTTP
        │
        ▼
Session Service
        │
        ▼
      Redis
```

При login Auth Service:

```text
1. Проверяет email/password
2. Создаёт session_id
3. Генерирует refresh token
4. Вычисляет SHA-256 hash refresh token
5. Вызывает POST /v1/sessions
6. Возвращает access + refresh tokens клиенту
```

При refresh:

```text
1. Извлекает session_id из refresh token
2. Вычисляет hash текущего refresh token
3. Проверяет сессию через Session Service
4. Генерирует новый token pair
5. Вызывает rotate
6. Старый refresh token становится невалидным
```

При logout:

```text
DELETE /v1/sessions/{session_id}
```

При logout everywhere:

```text
DELETE /v1/users/{user_id}/sessions
```

---

## Security

Session Service не хранит исходные refresh-токены.

В Redis сохраняется только:

```text
SHA-256(refresh_token)
```

Это уменьшает риск компрометации действующих refresh-токенов при утечке содержимого Redis.

Для межсервисных запросов используется `X-Internal-Api-Key`.

В production рекомендуется дополнительно ограничивать сетевой доступ к сервису средствами Kubernetes NetworkPolicy, firewall или private network.

---

## Testing

Тесты собираются вместе с проектом через CMake.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Основные сценарии тестирования:

```text
create session
verify session
rotate refresh token
list active sessions
revoke session
logout everywhere
session limit
```

---

## Health Check

Доступны два health endpoints:

```http
GET /health
GET /healthz
```

Пример:

```bash
curl http://localhost:8081/health
```

---

## Example Request

```bash
curl -X POST http://localhost:8081/v1/sessions \
  -H 'Content-Type: application/json' \
  -H 'X-Internal-Api-Key: change-me' \
  -d '{
    "session_id": "2e070e3a-6e95-45e5-9c0e-d28897f60d1c",
    "user_id": 42,
    "token_hash": "abc123",
    "device_id": "chrome-windows",
    "device_name": "Chrome on Windows",
    "ip_address": "127.0.0.1",
    "user_agent": "curl",
    "ttl_seconds": 2592000
  }'
```
