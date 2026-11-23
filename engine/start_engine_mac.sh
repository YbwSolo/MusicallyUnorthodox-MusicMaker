#!/bin/bash
# Starts the song engine and leaves it running. Everything it prints goes to the log the plugin shows.
#   start_engine_mac.sh <engine folder> <host> <port> <log file>
DIR="$1"; HOST="${2:-127.0.0.1}"; PORT="${3:-8001}"; LOG="${4:-/tmp/mu-engine.log}"
export PATH="$HOME/.local/bin:$HOME/.cargo/bin:/opt/homebrew/bin:/usr/local/bin:$PATH"
export TOKENIZERS_PARALLELISM="false"
export ACESTEP_INIT_LLM="${ACESTEP_INIT_LLM:-auto}"
if [ "$(uname)" = "Darwin" ] && [ "$(uname -m)" = "arm64" ]; then
  export ACESTEP_LM_BACKEND="mlx"
fi

echo "Starting the engine: $(date)" > "$LOG"
cd "$DIR" 2>>"$LOG" || { echo "The engine folder is missing: $DIR" >> "$LOG"; exit 1; }

for PY in "$DIR/python_embedded/bin/python3.11" "$DIR/python_embeded/bin/python3.11"; do
  if [ -x "$PY" ]; then
    exec "$PY" "$DIR/acestep/api_server.py" --host "$HOST" --port "$PORT" >> "$LOG" 2>&1
  fi
done

if ! command -v uv >/dev/null 2>&1; then
  echo "The Python manager (uv) is missing. Run Set up the engine in Settings." >> "$LOG"
  exit 1
fi
if [ ! -d "$DIR/.venv" ]; then
  echo "Finishing the engine's setup..." >> "$LOG"
  uv sync >> "$LOG" 2>&1 || { echo "The engine's setup did not finish. Run Set up the engine in Settings." >> "$LOG"; exit 1; }
fi
exec uv run --no-sync acestep-api --host "$HOST" --port "$PORT" >> "$LOG" 2>&1
