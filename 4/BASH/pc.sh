#!/usr/bin/env bash
PROC_FILE="/proc/modlist"

#Productor
(
    for i in $(seq 1 1300); do
        echo "add $((r * 1000 + i))" > "$PROC_FILE" 2>/dev/null || true
    done
) &

sleep 0.001
# Consumidor
(
    for i in $(seq 1 1300); do
        echo "remove $((r * 1000 + i))" > "$PROC_FILE" 2>/dev/null || true
    done
) &

echo "Lista final: "
cat "$PROC_FILE" || true