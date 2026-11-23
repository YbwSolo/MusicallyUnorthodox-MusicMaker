#!/bin/bash
# Stops whatever is answering on the engine's port.
#   stop_engine_mac.sh <port>
PORT="${1:-8001}"
PIDS="$(lsof -ti "tcp:$PORT" -sTCP:LISTEN 2>/dev/null)"
[ -z "$PIDS" ] && exit 0
kill $PIDS 2>/dev/null
for _ in 1 2 3 4 5 6; do
  sleep 0.5
  PIDS="$(lsof -ti "tcp:$PORT" -sTCP:LISTEN 2>/dev/null)"
  [ -z "$PIDS" ] && exit 0
done
kill -9 $PIDS 2>/dev/null
exit 0
