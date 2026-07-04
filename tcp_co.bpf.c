#include "vmlinux.h"

#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_endian.h>

char LICENSE[] SEC("license") = "GPL";

// fentry: dispara na ENTRADA da função tcp_rcv_established, toda vez
// que um pacote é processado numa conexão TCP já estabelecida.
// Assinatura real do kernel: void tcp_rcv_established(struct sock *sk, struct sk_buff *skb)
SEC("fentry/tcp_rcv_established")
int BPF_PROG(trace_tcp_rcv_established, struct sock *sk, struct sk_buff *skb)
{
    // struct sock é o "objeto base"; tcp_sock estende ele.
    // Fazemos o cast e usamos BPF_CORE_READ para ler os campos de forma
    // portável entre versões de kernel (CO-RE resolve os offsets certos
    // em tempo de load, usando o BTF do kernel de destino).
    struct tcp_sock *tp = (struct tcp_sock *)sk;

    __u32 snd_cwnd   = BPF_CORE_READ(tp, snd_cwnd);
    __u32 ssthresh   = BPF_CORE_READ(tp, snd_ssthresh);
    __u32 srtt_us    = BPF_CORE_READ(tp, srtt_us) >> 3; // srtt_us é guardado em escala 8x
    __u16 dport      = BPF_CORE_READ(sk, __sk_common.skc_dport);

    // bpf_ntohs porque a porta vem em network byte order
    bpf_printk("tcp_rcv_established: dport=%d cwnd=%d ssthresh=%d srtt_us=%d",
               bpf_ntohs(dport), snd_cwnd, ssthresh, srtt_us);

    return 0;
}