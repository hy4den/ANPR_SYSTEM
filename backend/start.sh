#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="$SCRIPT_DIR/.env"

# Load .env if present
if [[ -f "$ENV_FILE" ]]; then
    set -a
    # shellcheck disable=SC1090
    source "$ENV_FILE"
    set +a
else
    echo "[WARN] .env not found — using .env.example values"
    set -a
    source "$SCRIPT_DIR/.env.example"
    set +a
fi

if [[ -z "${ADMIN_API_TOKEN:-}" ]]; then
    echo "[WARN] ADMIN_API_TOKEN not set — using default dev token"
    export ADMIN_API_TOKEN="anpr-dev-admin-token"
fi

TLS_CERT_FILE="${TLS_CERT_FILE:-$SCRIPT_DIR/certs/dev-cert.pem}"
TLS_KEY_FILE="${TLS_KEY_FILE:-$SCRIPT_DIR/certs/dev-key.pem}"
TLS_PORT="${TLS_PORT:-8443}"
export TLS_CERT_FILE TLS_KEY_FILE TLS_PORT

if [[ ! -f "$TLS_CERT_FILE" || ! -f "$TLS_KEY_FILE" ]]; then
    mkdir -p "$(dirname "$TLS_CERT_FILE")"
    if command -v openssl >/dev/null 2>&1; then
        echo "[INFO] Generating self-signed TLS cert for local HTTPS..."
        openssl req -x509 -newkey rsa:2048 -sha256 -nodes \
            -keyout "$TLS_KEY_FILE" \
            -out "$TLS_CERT_FILE" \
            -subj "/CN=localhost" \
            -days 3650 >/dev/null 2>&1
    else
        echo "[WARN] openssl not found — HTTPS listener may be disabled"
    fi
fi

BINARY="$SCRIPT_DIR/build/anpr_backend"
if [[ ! -x "$BINARY" ]]; then
    echo "[ERROR] Binary not found: $BINARY"
    echo "Build first:  cd build && make -j$(nproc)"
    exit 1
fi

echo "[INFO] Starting ANPR backend on port 8000..."
echo "[INFO] HTTPS target port is ${TLS_PORT} (cert: ${TLS_CERT_FILE})"
exec "$BINARY"
