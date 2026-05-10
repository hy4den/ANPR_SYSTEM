#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BACKEND_DIR="$ROOT_DIR/backend"
FRONTEND_DIR="$ROOT_DIR/frontend"
LOG_DIR="$ROOT_DIR/.logs"
PID_DIR="$ROOT_DIR/.run"

BACKEND_PORT="${BACKEND_PORT:-8000}"
FRONTEND_PORT="${FRONTEND_PORT:-5173}"
START_NGROK="${START_NGROK:-1}"

mkdir -p "$LOG_DIR" "$PID_DIR"

port_in_use() {
  local port="$1"
  lsof -nP -iTCP:"$port" -sTCP:LISTEN >/dev/null 2>&1
}

if port_in_use "$BACKEND_PORT"; then
  echo "[ERROR] Port $BACKEND_PORT is already in use."
  echo "        Stop existing process first, then run ./start.sh again."
  exit 1
fi

if port_in_use "$FRONTEND_PORT"; then
  echo "[ERROR] Port $FRONTEND_PORT is already in use."
  echo "        Stop existing process first, then run ./start.sh again."
  exit 1
fi

echo "[INFO] Starting backend..."
(
  cd "$BACKEND_DIR"
  nohup bash start.sh > "$LOG_DIR/backend.log" 2>&1 &
  echo $! > "$PID_DIR/backend.pid"
)

echo "[INFO] Starting frontend..."
(
  cd "$FRONTEND_DIR"
  nohup npm run dev -- --host 0.0.0.0 --port "$FRONTEND_PORT" > "$LOG_DIR/frontend.log" 2>&1 &
  echo $! > "$PID_DIR/frontend.pid"
)

NGROK_URL=""
if [[ "$START_NGROK" == "1" ]]; then
  if command -v ngrok >/dev/null 2>&1; then
    echo "[INFO] Starting ngrok tunnel..."
    nohup ngrok http "$FRONTEND_PORT" > "$LOG_DIR/ngrok.log" 2>&1 &
    echo $! > "$PID_DIR/ngrok.pid"
    sleep 2
    NGROK_URL="$(curl -sS http://127.0.0.1:4040/api/tunnels | python3 -c 'import sys, json; d=json.load(sys.stdin); print(next((t.get("public_url","") for t in d.get("tunnels",[]) if str(t.get("public_url","")).startswith("https://")), ""))' 2>/dev/null || true)"
  else
    echo "[WARN] ngrok not found. Install it or run with START_NGROK=0."
  fi
fi

echo ""
echo "Started:"
echo "  Backend : http://localhost:$BACKEND_PORT"
echo "  Frontend: http://localhost:$FRONTEND_PORT"
if [[ -n "$NGROK_URL" ]]; then
  echo "  ngrok   : $NGROK_URL"
elif [[ "$START_NGROK" == "1" ]]; then
  echo "  ngrok   : check $LOG_DIR/ngrok.log"
fi
echo ""
echo "Logs:"
echo "  $LOG_DIR/backend.log"
echo "  $LOG_DIR/frontend.log"
[[ "$START_NGROK" == "1" ]] && echo "  $LOG_DIR/ngrok.log"
