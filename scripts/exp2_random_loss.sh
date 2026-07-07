#!/usr/bin/env bash
# exp2_random_loss.sh — Experimento 2 (Parte 4)
# Objetivo: aplicar perda aleatória via tc netem e observar impacto em
# retransmissões, redução de cwnd e alterações no ssthresh.
#
# Uso: ./exp2_random_loss.sh [algoritmo] [perda] [duracao_segundos]
# Ex:  ./exp2_random_loss.sh cubic 1% 30

set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$DIR/lib.sh"

ALGO="${1:-cubic}"
LOSS="${2:-1%}"
DURATION="${3:-30}"

mkdir -p results
OUT="results/exp2_loss_${LOSS//%/pct}_${ALGO}.csv"

echo "=== Experimento 2: Perda aleatória ($LOSS, algoritmo=$ALGO, ${DURATION}s) ==="
echo "Pré-requisito: rode 'iperf3 -s' em outro terminal antes de continuar."
read -rp "Pressione Enter quando o servidor iperf3 estiver pronto..."

set_netem loss "$LOSS"
start_capture "$OUT"

iperf3 -c 127.0.0.1 -C "$ALGO" -t "$DURATION"

stop_capture
clear_netem
echo "=== Concluído. Dados em $OUT ==="
echo "Gere os gráficos com:"
echo "  python3 analyze/plot_tcp_co.py cwnd $OUT"
echo "  python3 analyze/plot_tcp_co.py retrans $OUT"
echo ""
echo "Também vale rodar a detecção automática de eventos (Desafio Extra):"
echo "  python3 analyze/detect_events.py $OUT"