#!/usr/bin/env bash
# exp3_high_rtt.sh — Experimento 3 (Parte 4)
# Objetivo: variar o RTT (100ms, 200ms, 500ms) e observar impacto no
# throughput e no comportamento do algoritmo de congestionamento.
#
# Uso: ./exp3_high_rtt.sh [algoritmo] [duracao_por_delay]
# Ex:  ./exp3_high_rtt.sh cubic 20
#
# Roda os 3 delays em sequência, um CSV separado por delay.

set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$DIR/lib.sh"

ALGO="${1:-cubic}"
DURATION="${2:-20}"
DELAYS=(100ms 200ms 500ms)

mkdir -p results

echo "=== Experimento 3: RTT elevado (algoritmo=$ALGO) ==="
echo "Pré-requisito: rode 'iperf3 -s' em outro terminal antes de continuar."
read -rp "Pressione Enter quando o servidor iperf3 estiver pronto..."

for DELAY in "${DELAYS[@]}"; do
    OUT="results/exp3_rtt_${DELAY}_${ALGO}.csv"
    echo ""
    echo "--- Delay=$DELAY ---"
    set_netem delay "$DELAY"
    start_capture "$OUT"

    iperf3 -c 127.0.0.1 -C "$ALGO" -t "$DURATION"

    stop_capture
    clear_netem
    sleep 2   # respiro entre os testes para não misturar estado residual
done

echo ""
echo "=== Concluído. Arquivos gerados: ==="
ls -1 results/exp3_rtt_*_"${ALGO}".csv
echo ""
echo "Gere os gráficos de RTT com, por exemplo:"
echo "  python3 analyze/plot_tcp_co.py rtt results/exp3_rtt_100ms_${ALGO}.csv"