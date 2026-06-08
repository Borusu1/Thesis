#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BACKEND_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
COMPOSE_FILE="${BACKEND_DIR}/docker-compose.yml"
POSTGRES_SERVICE="postgres"
POSTGRES_USER="warehouse"
POSTGRES_DB_PREFIX="warehouse_test"
TEST_DB_NAME="${POSTGRES_DB_PREFIX}_$(date +%s)_$RANDOM"

cleanup() {
  docker compose -f "${COMPOSE_FILE}" exec -T "${POSTGRES_SERVICE}" \
    psql -U "${POSTGRES_USER}" -d postgres \
    -c "SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE datname = '${TEST_DB_NAME}' AND pid <> pg_backend_pid();" \
    >/dev/null 2>&1 || true
  docker compose -f "${COMPOSE_FILE}" exec -T "${POSTGRES_SERVICE}" \
    dropdb -U "${POSTGRES_USER}" --if-exists "${TEST_DB_NAME}" \
    >/dev/null 2>&1 || true
}

trap cleanup EXIT

docker compose -f "${COMPOSE_FILE}" up -d "${POSTGRES_SERVICE}" >/dev/null

docker compose -f "${COMPOSE_FILE}" exec -T "${POSTGRES_SERVICE}" \
  createdb -U "${POSTGRES_USER}" "${TEST_DB_NAME}"

export APP_ENV=test
export TEST_DATABASE_URL="postgresql+asyncpg://${POSTGRES_USER}:${POSTGRES_USER}@127.0.0.1:5432/${TEST_DB_NAME}"
export DATABASE_URL="${TEST_DATABASE_URL}"
export JWT_SECRET_KEY="test-secret-key-with-safe-length-123"

cd "${BACKEND_DIR}"
./.venv/bin/alembic upgrade head >/dev/null

if [ "$#" -eq 0 ]; then
  exec ./.venv/bin/pytest tests
else
  exec ./.venv/bin/pytest "$@"
fi
