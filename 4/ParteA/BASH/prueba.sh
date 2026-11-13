#!/usr/bin/env bash
PROC_FILE="/proc/modlist"
WORKERS=8
ELEMS_POR_WORKER=600
NUM_CATS=50

#Restauramos estado por defecto
echo "cleanup" > "$PROC_FILE" 2>/dev/null || true

for w in $(seq 1 $WORKERS); do
    base=$(((w-1)*ELEMS_POR_WORKER))
    (
        #Productor
        for i in $(seq 1 $ELEMS_POR_WORKER); do
            echo "add $((base + i))" > "$PROC_FILE" 2>/dev/null || true
        done
        # Consumidor (Borramos solo los elementos pares)
        for i in $(seq 1 $((ELEMS_POR_WORKER/2))); do
            echo "remove $((base + i*2))" > "$PROC_FILE" 2>/dev/null || true
        done
    ) &
done

#Metemos cats, aunque no los capturemos, para que veamos que no se rompa el módulo a la vez que tenemos add y delete.
for w in $(seq 1 $WORKERS); do
    (
        for i in $(seq 1 $NUM_CATS); do
            cat "$PROC_FILE" >/dev/null
        done
    ) &
done

#Calculamos el resultado que deberíamos esperar(solo impares)
expected=$(
    for w in $(seq 1 $WORKERS); do
        base=$(((w-1)*ELEMS_POR_WORKER))
        for ((i = 1; i <= ELEMS_POR_WORKER; i += 2)); do
            echo $((base + i))
        done
    done | sort -n
)

wait

#Resultado ordenado.
resultado=$(cat "$PROC_FILE" 2>/dev/null | sort -n)

echo "$resultado" > resultado.txt
echo "$expected" > expected.txt

([[ "$expected" == "$resultado" ]] && echo "Test passed: Resultado esperado coincide con el resultado obtenido.") || echo "Error: Resultado esperado no coincide con el resultado obtenido."