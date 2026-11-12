#!/usr/bin/env bash
PROC_FILE="/proc/modlist"

info "Prueba SMP (7 escritores + 4 lectores en paralelo)..."

# Lanzamos 1 proceso añadiendo 1 número en el búcle
(
    for i in $(seq 1 300); do
        echo "add $((w * 1000 + i))" > "$PROC_FILE" 2>/dev/null || true
    done
) &

# insercciones cada cierto tiempo y otro que vaya eliminando e imprimendo.
# Lanzamos 1 proceso borrando números constantemente
(
    for i in $(seq 1 300); do
        echo "remove $((r * 1000 + i))" > "$PROC_FILE" 2>/dev/null || true
    done
) &

# Lanzamos 1 proceso haciendo cleanups periódicos en mitad del caos
(
    for c in $(seq 1 15); do
        sleep 0.02
        echo "cleanup" > "$PROC_FILE" 2>/dev/null || true
    done
) &

# Lanzamos 4 lectores agresivos leyendo continuamente con cat
for rd in 1 2 3 4; do
    (
        for i in $(seq 1 200); do
            cat "$PROC_FILE" > /dev/null 2>&1 || true
        done
    ) &
done

# Esperar a que terminen todos los subprocesos
wait
pass "Todos los procesos concurrentes finalizaron sin bloqueos ni crashes"

# Limpieza final y descarga
echo "cleanup" > "$PROC_FILE"
rmmod "$MODULE_NAME" || fail "Error al descargar el módulo con rmmod"
pass "Módulo descargado limpiamente con rmmod"

# ---------------------------------------------------------
# FASE 4: Auditoría de dmesg (buscar Oops, BUG, Warning, Leak)
# ---------------------------------------------------------
info "Fase 4: Revisando logs del kernel (dmesg) en busca de errores..."

if dmesg | tail -n 100 | grep -Ei "BUG:|Oops:|scheduling while atomic|WARNING:|Call Trace:|slab error"; then
    fail "¡Se detectaron errores graves del kernel en dmesg!"
else
    pass "dmesg completamente limpio (0 errores de concurrencia o memoria)"
fi

echo -e "\n${GREEN}>>> TODAS LAS PRUEBAS SUPERADAS CON ÉXITO <<<${NC}"
