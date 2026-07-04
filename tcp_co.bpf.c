// tcp_co.bpf.c
// Etapa 2 do TCP Congestion Observatory (TCP-CO)
// Objetivo: implementar o mapa por conexão (BPF_MAP_TYPE_HASH) usando
// exatamente as structs flow_key/tcp_metrics do enunciado, atualizado
// a cada pacote (fentry/tcp_rcv_established) e a cada retransmissão
// (fentry/tcp_retransmit_skb).

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

#ifndef AF_INET
    #define AF_INET 2
#endif

char LICENSE[] SEC("license") = "GPL";

// ---- Structs exatamente como no enunciado ----

struct flow_key {
    __u32 src_ip;
    __u32 dst_ip;
    __u16 src_port;
    __u16 dst_port;
};

struct tcp_metrics {
    __u32 snd_cwnd;
    __u32 ssthresh;
    __u32 srtt;
    __u32 retransmissions;
    __u32 duplicate_acks;
    __u64 bytes_acked;
};

// ---- Mapa por conexão ----
// Escolhido HASH porque o número de conexões simultâneas é imprevisível
// e não precisamos de ordenação — só lookup/update por chave.
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 4096);
    __type(key, struct flow_key);
    __type(value, struct tcp_metrics);
} tcp_flows SEC(".maps");

// Monta a flow_key a partir de um struct sock*.
// Nota: só tratamos IPv4 aqui (skc_family == AF_INET) — o enunciado usa
// __u32 para src_ip/dst_ip, então IPv6 fica fora de escopo por design.
static __always_inline void build_flow_key(struct sock *sk, struct flow_key *key)
{
    __builtin_memset(key, 0, sizeof(*key));

    key->src_ip   = BPF_CORE_READ(sk, __sk_common.skc_rcv_saddr);
    key->dst_ip   = BPF_CORE_READ(sk, __sk_common.skc_daddr);
    key->dst_port = bpf_ntohs(BPF_CORE_READ(sk, __sk_common.skc_dport));

    // skc_num já vem em host byte order (diferente de skc_dport!)
    key->src_port = BPF_CORE_READ(sk, __sk_common.skc_num);
}

SEC("fentry/tcp_rcv_established")
int BPF_PROG(trace_tcp_rcv_established, struct sock *sk, struct sk_buff *skb)
{
    struct tcp_sock *tp = (struct tcp_sock *)sk;

    // Só nos interessa IPv4 nesta etapa
    if (BPF_CORE_READ(sk, __sk_common.skc_family) != AF_INET)
        return 0;

    struct flow_key key;
    build_flow_key(sk, &key);

    struct tcp_metrics *m = bpf_map_lookup_elem(&tcp_flows, &key);
    struct tcp_metrics zero = {};

    if (!m) {
        // Primeira vez que vemos esse fluxo: cria a entrada
        bpf_map_update_elem(&tcp_flows, &key, &zero, BPF_ANY);
        m = bpf_map_lookup_elem(&tcp_flows, &key);
        if (!m)
            return 0; // não deveria acontecer, mas o verifier exige o check
    }

    // Atualiza as métricas "de estado atual" (sobrescrevem a cada pacote)
    m->snd_cwnd    = BPF_CORE_READ(tp, snd_cwnd);
    m->ssthresh    = BPF_CORE_READ(tp, snd_ssthresh);
    m->srtt        = BPF_CORE_READ(tp, srtt_us) >> 3; // escala 8x, ver docs do kernel
    m->bytes_acked = BPF_CORE_READ(tp, bytes_acked);

    return 0;
}

SEC("fentry/tcp_retransmit_skb")
int BPF_PROG(trace_tcp_retransmit_skb, struct sock *sk, struct sk_buff *skb, int segs)
{
    if (BPF_CORE_READ(sk, __sk_common.skc_family) != AF_INET)
        return 0;

    struct flow_key key;
    build_flow_key(sk, &key);

    struct tcp_metrics *m = bpf_map_lookup_elem(&tcp_flows, &key);
    if (!m) {
        // Retransmissão de um fluxo que ainda não passou por
        // tcp_rcv_established (raro, mas possível na prática) — criamos
        // a entrada mesmo assim para não perder o evento.
        struct tcp_metrics zero = {};
        bpf_map_update_elem(&tcp_flows, &key, &zero, BPF_ANY);
        m = bpf_map_lookup_elem(&tcp_flows, &key);
        if (!m)
            return 0;
    }

    // retransmissions é acumulativo (contador de eventos), diferente das
    // métricas "de estado atual" acima que são sobrescritas
    __sync_fetch_and_add(&m->retransmissions, 1);

    return 0;
}