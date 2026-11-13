#!/bin/sh
# Agent benchmark (SC-001, SC-008): bench/run_agent.sh --lang vo|python [--model M] [--only NN,NN]
exec python3 "$(dirname "$0")/agent.py" "$@"
