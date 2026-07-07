#!/usr/bin/env bash
# exp1_cwnd_growth.sh — Experimento 1 (Parte 4)
# Objetivo: observar a evolução de snd_cwnd em condições ideais de rede
# (sem netem), identificando Slow Start e a transição para Congestion Avoidance.
#
# Uso: ./exp1_cwnd_growth.sh [algoritmo] [duracao_segundos]
# Ex:  ./exp1_cwnd_growth.sh cubic 30

set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$DIR/lib.sh"

ALGO="${1:-cubic}"
DURATION="${2:-30}"

mkdir -p results
OUT="results/exp1_cwnd_growth_${ALGO}.csv"

echo "=== Experimento 1: Crescimento da cwnd (algoritmo=$ALGO, ${DURATION}s) ==="
echo "Pré-requisito: rode 'iperf3 -s' em outro terminal antes de continuar."
read -rp "Pressione Enter quando o servidor iperf3 estiver pronto..."

clear_netem   # experimento 1 é em condições ideais, de propósito
start_capture "$OUT"

iperf3 -c 127.0.0.1 -C "$ALGO" -t "$DURATION"

stop_capture
echo "=== Concluído. Dados em $OUT ==="
echo "Gere o gráfico com:"
echo "  python3 analyze/plot_tcp_co.py cwnd $OUT"