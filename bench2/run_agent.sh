#!/bin/sh
# Benchmark v2: bench2/run_agent.sh --lang vo|python [--set tasks|features] [--model M] [--only ID,ID]
exec python3 "$(dirname "$0")/agent.py" "$@"
