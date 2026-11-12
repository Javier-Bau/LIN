#!/usr/bin/env bash
PROC_FILE="/proc/modlist"

for i in $(seq 1 300); do
    echo "remove $((r * 1000 + i))" > "$PROC_FILE" 2>/dev/null || true
done
