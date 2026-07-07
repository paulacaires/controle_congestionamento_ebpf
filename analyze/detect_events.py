#!/usr/bin/env python3
"""
detect_events.py — Desafio Extra (bônus): detecção automática de eventos
de congestionamento.

Regra do enunciado: se cwnd reduzir mais de 50% em relação à amostra
anterior E o contador de retransmissões tiver aumentado no mesmo
intervalo, emite um alerta.

Requisitos:
    pip install pandas --break-system-packages

Uso:
    python3 detect_events.py results/exp2_loss_1pct_cubic.csv
"""
import sys
import pandas as pd

KEY_COLS = ["src_ip", "src_port", "dst_ip", "dst_port"]


def detect(csv_path, drop_threshold=0.5):
    df = pd.read_csv(csv_path).sort_values("timestamp")

    alerts = []
    for _, flow_df in df.groupby(KEY_COLS):
        flow_df = flow_df.reset_index(drop=True)
        for i in range(1, len(flow_df)):
            prev_cwnd = flow_df.loc[i - 1, "snd_cwnd"]
            cur_cwnd = flow_df.loc[i, "snd_cwnd"]
            prev_retrans = flow_df.loc[i - 1, "retransmissions"]
            cur_retrans = flow_df.loc[i, "retransmissions"]

            if prev_cwnd == 0:
                continue

            drop_ratio = (prev_cwnd - cur_cwnd) / prev_cwnd
            retrans_increased = cur_retrans > prev_retrans

            if drop_ratio > drop_threshold and retrans_increased:
                alerts.append({
                    "timestamp": flow_df.loc[i, "timestamp"],
                    "flow": tuple(flow_df.loc[i, KEY_COLS]),
                    "cwnd_antes": int(prev_cwnd),
                    "cwnd_depois": int(cur_cwnd),
                    "queda_pct": round(drop_ratio * 100, 1),
                    "retransmissoes": int(cur_retrans),
                })
    return alerts


def main():
    if len(sys.argv) < 2:
        print("Uso: python3 detect_events.py <arquivo.csv>")
        sys.exit(1)

    alerts = detect(sys.argv[1])
    if not alerts:
        print("Nenhum evento de congestionamento detectado.")
        return

    print(f"{len(alerts)} evento(s) de congestionamento detectado(s):\n")
    for a in alerts:
        print(f"[ALERTA] t={a['timestamp']} fluxo={a['flow']} "
              f"cwnd {a['cwnd_antes']}→{a['cwnd_depois']} "
              f"(queda de {a['queda_pct']}%), retransmissões={a['retransmissoes']}")


if __name__ == "__main__":
    main()