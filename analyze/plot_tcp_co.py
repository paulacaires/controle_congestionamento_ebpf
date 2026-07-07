#!/usr/bin/env python3
"""
plot_tcp_co.py — gera os gráficos da Parte 5 do TCP-CO a partir dos CSVs
produzidos pelo loader (tcp_co, Etapa 3).

Requisitos:
    pip install pandas matplotlib --break-system-packages

Uso:
    python3 plot_tcp_co.py cwnd     results/exp1_cwnd_growth_cubic.csv
    python3 plot_tcp_co.py rtt      results/exp3_rtt_100ms_cubic.csv
    python3 plot_tcp_co.py retrans  results/exp2_loss_1pct_cubic.csv
    python3 plot_tcp_co.py compare  results/exp4_compare_reno.csv results/exp4_compare_cubic.csv
"""
import argparse
import sys
import pandas as pd
import matplotlib.pyplot as plt

KEY_COLS = ["src_ip", "src_port", "dst_ip", "dst_port"]


def load(csv_path):
    df = pd.read_csv(csv_path)
    df = df.sort_values("timestamp")
    df["t"] = df["timestamp"] - df["timestamp"].min()
    return df


def pick_main_flow(df):
    """
    O CSV pode conter mais de um fluxo. Isso é especialmente comum quando
    cliente e servidor do iperf3 rodam NO MESMO HOST (dois terminais):
    o hook fentry captura os DOIS lados da conexão, cada um com sua
    própria flow_key — o socket do cliente (que envia os dados, cujo
    snd_cwnd cresce de verdade) e o socket do servidor (que só manda
    ACKs de volta, cujo snd_cwnd fica praticamente parado).

    Critério antigo (mais amostras) escolhia errado nesse cenário: como
    o servidor RECEBE pacotes de dados com muito mais frequência do que
    o cliente recebe ACKs, o socket do servidor gerava mais linhas no
    CSV, mesmo sendo o lado "parado".

    Critério novo: escolhemos o fluxo com maior VARIAÇÃO de bytes_acked
    (max - min). bytes_acked só cresce de forma significativa no lado
    que está de fato enviando dados em volume — o lado que só manda
    ACKs tem bytes_acked praticamente constante. Isso identifica o
    fluxo certo sem depender de hardcodar a porta do iperf3 (5201),
    então funciona também com netcat ou aplicação própria.
    """
    variation = (
        df.groupby(KEY_COLS)["bytes_acked"]
        .agg(lambda s: s.max() - s.min())
        .sort_values(ascending=False)
    )
    if len(variation) > 1:
        print(f"[aviso] {len(variation)} fluxos encontrados no CSV; "
              f"usando o de maior variação de bytes_acked: {variation.index[0]} "
              f"(delta={variation.iloc[0]})",
              file=sys.stderr)
    main_key = variation.index[0]
    mask = pd.Series(True, index=df.index)
    for col, val in zip(KEY_COLS, main_key):
        mask &= (df[col] == val)
    return df[mask]


def plot_cwnd(csv_path, out_png):
    df = pick_main_flow(load(csv_path))
    plt.figure(figsize=(9, 5))
    plt.plot(df["t"], df["snd_cwnd"], marker="o", markersize=3, label="snd_cwnd")
    plt.plot(df["t"], df["ssthresh"], linestyle="--", label="ssthresh")
    plt.xlabel("Tempo (s)")
    plt.ylabel("Segmentos")
    plt.title(f"Janela de Congestionamento × Tempo\n({csv_path})")
    plt.legend()
    plt.grid(alpha=0.3)
    plt.tight_layout()
    plt.savefig(out_png, dpi=150)
    print(f"Gráfico salvo em {out_png}")


def plot_rtt(csv_path, out_png):
    df = pick_main_flow(load(csv_path))
    plt.figure(figsize=(9, 5))
    plt.plot(df["t"], df["srtt_us"] / 1000.0, color="tab:orange")
    plt.xlabel("Tempo (s)")
    plt.ylabel("RTT suavizado (ms)")
    plt.title(f"RTT × Tempo\n({csv_path})")
    plt.grid(alpha=0.3)
    plt.tight_layout()
    plt.savefig(out_png, dpi=150)
    print(f"Gráfico salvo em {out_png}")


def plot_retrans(csv_path, out_png):
    df = pick_main_flow(load(csv_path))
    plt.figure(figsize=(9, 5))
    plt.step(df["t"], df["retransmissions"], where="post", color="tab:red")
    plt.xlabel("Tempo (s)")
    plt.ylabel("Retransmissões acumuladas")
    plt.title(f"Retransmissões Acumuladas × Tempo\n({csv_path})")
    plt.grid(alpha=0.3)
    plt.tight_layout()
    plt.savefig(out_png, dpi=150)
    print(f"Gráfico salvo em {out_png}")


def plot_compare(csv_paths, out_png, metric):
    plt.figure(figsize=(9, 5))
    for path in csv_paths:
        df = pick_main_flow(load(path))
        algo = df["algo"].mode().iloc[0] if "algo" in df.columns else path
        plt.plot(df["t"], df[metric], label=f"{algo}")
    plt.xlabel("Tempo (s)")
    plt.ylabel(metric)
    plt.title(f"Comparação entre algoritmos — {metric}")
    plt.legend()
    plt.grid(alpha=0.3)
    plt.tight_layout()
    plt.savefig(out_png, dpi=150)
    print(f"Gráfico salvo em {out_png}")


def main():
    parser = argparse.ArgumentParser(description="Gera gráficos do TCP-CO (Parte 5)")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p1 = sub.add_parser("cwnd", help="Gráfico 1: cwnd × tempo")
    p1.add_argument("csv")
    p1.add_argument("--out", default="cwnd_tempo.png")

    p2 = sub.add_parser("rtt", help="Gráfico 2: RTT × tempo")
    p2.add_argument("csv")
    p2.add_argument("--out", default="rtt_tempo.png")

    p3 = sub.add_parser("retrans", help="Gráfico 3: retransmissões acumuladas × tempo")
    p3.add_argument("csv")
    p3.add_argument("--out", default="retransmissoes_tempo.png")

    p4 = sub.add_parser("compare", help="Gráfico 4: comparação entre algoritmos")
    p4.add_argument("csvs", nargs="+")
    p4.add_argument("--metric", default="snd_cwnd",
                     choices=["snd_cwnd", "srtt_us", "retransmissions"])
    p4.add_argument("--out", default="comparacao_algoritmos.png")

    args = parser.parse_args()

    if args.cmd == "cwnd":
        plot_cwnd(args.csv, args.out)
    elif args.cmd == "rtt":
        plot_rtt(args.csv, args.out)
    elif args.cmd == "retrans":
        plot_retrans(args.csv, args.out)
    elif args.cmd == "compare":
        plot_compare(args.csvs, args.out, args.metric)


if __name__ == "__main__":
    main()