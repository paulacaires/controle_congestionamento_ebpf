#!/usr/bin/env bash
# exp4_algo_comparison.sh — Experimento 4 (Parte 4)
# Objetivo: comparar dois algoritmos (o enunciado pede escolher 2 entre
# Reno/Cubic/BBR) sob a MESMA condição de rede, para comparação justa.
#
# Uso: ./exp4_algo_comparison.sh [algo_A] [algo_B] [duracao] ["condicao netem"]
# Ex:  ./exp4_algo_comparison.sh reno cubic 30 "delay 50ms loss 1%"

set -euo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$DIR/lib.sh"

ALGO_A="${1:-reno}"
ALGO_B="${2:-cubic}"
DURATION="${3:-30}"
NETEM_COND="${4:-delay 50ms loss 1%}"

mkdir -p results

echo "=== Experimento 4: Comparação $ALGO_A vs $ALGO_B ==="
echo "Condição de rede aplicada a ambos: $NETEM_COND"
echo "Pré-requisito: rode 'iperf3 -s' em outro terminal antes de continuar."
read -rp "Pressione Enter quando o servidor iperf3 estiver pronto..."

for ALGO in "$ALGO_A" "$ALGO_B"; do
    OUT="results/exp4_compare_${ALGO}.csv"
    echo ""
    echo "--- Testando algoritmo=$ALGO ---"
    # shellcheck disable=SC2086 -- queremos que $NETEM_COND seja splitado em argumentos
    set_netem $NETEM_COND
    start_capture "$OUT"

    iperf3 -c 127.0.0.1 -C "$ALGO" -t "$DURATION"

    stop_capture
    clear_netem
    sleep 2
done

echo ""
echo "=== Concluído. Arquivos gerados: ==="
echo "  results/exp4_compare_${ALGO_A}.csv"
echo "  results/exp4_compare_${ALGO_B}.csv"
echo ""
echo "Gere o gráfico comparativo com:"
echo "  python3 analyze/plot_tcp_co.py compare results/exp4_compare_${ALGO_A}.csv results/exp4_compare_${ALGO_B}.csv --metric snd_cwnd"