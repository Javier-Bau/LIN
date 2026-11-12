#!/usr/bin/env bash
PROC_FILE="/proc/modlist"

#Productor
(
    for i in $(seq 1 300); do
        echo "add $((r * 1000 + i))" > "$PROC_FILE" 2>/dev/null || true
    done
) &

# Consumidor
(
    for i in $(seq 1 300); do
        echo "remove $((r * 1000 + i))" > "$PROC_FILE" 2>/dev/null || true
        cat "$PROC_FILE" > /dev/null 2>&1 || true
    done
) &