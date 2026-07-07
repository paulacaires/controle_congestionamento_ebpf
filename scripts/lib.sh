#!/usr/bin/env bash
# lib.sh — funções compartilhadas pelos scripts de experimento do TCP-CO.
# Uso: `source lib.sh` no início de cada script de experimento (exp1..exp4).
#
# Pressupõe que o binário tcp_co (Etapa 3) já foi compilado e está
# acessível via TCP_CO_BIN (por padrão, ./tcp_co no diretório atual).

TCP_CO_BIN="${TCP_CO_BIN:-./tcp_co}"
IFACE="${IFACE:-lo}"   # usamos loopback para simplificar; troque para
                        # a interface real (ex: eth0) se testar entre 2 máquinas

# Inicia o tcp_co em background, gravando no CSV informado.
# Define TCP_CO_PID globalmente para stop_capture() usar depois.
start_capture() {
    local out_csv="$1"
    sudo "$TCP_CO_BIN" "$out_csv" > /tmp/tcp_co_stdout.log 2>&1 &
    TCP_CO_PID=$!
    sleep 1  # dá tempo do programa anexar os hooks fentry antes do tráfego começar
    echo "[lib] tcp_co iniciado (PID=$TCP_CO_PID), gravando em $out_csv"
}

stop_capture() {
    if [[ -n "${TCP_CO_PID:-}" ]]; then
        sudo kill -INT "$TCP_CO_PID" 2>/dev/null || true
        wait "$TCP_CO_PID" 2>/dev/null || true
        echo "[lib] tcp_co finalizado"
        unset TCP_CO_PID
    fi
}

clear_netem() {
    sudo tc qdisc del dev "$IFACE" root 2>/dev/null || true
}

# set_netem delay 100ms
# set_netem loss 2%
# set_netem delay 50ms loss 1%
set_netem() {
    clear_netem
    echo "[lib] aplicando netem em $IFACE: $*"
    sudo tc qdisc add dev "$IFACE" root netem "$@"
}

# Garante limpeza mesmo se o script for interrompido com Ctrl+C
_lib_cleanup() {
    stop_capture
    clear_netem
}
trap _lib_cleanup EXIT