// tcp_co.c
// Loader user-space da Etapa 2.
// Diferente da Etapa 1 (que só lia trace_pipe), agora fazemos polling
// periódico do mapa tcp_flows e imprimimos + gravamos em CSV.
// O formato do CSV já é pensado para a Parte 5 (visualização) do trabalho.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <bpf/libbpf.h>
#include "tcp_co.skel.h"

// Precisa bater EXATAMENTE com as structs do .bpf.c
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

static volatile sig_atomic_t exiting = 0;
static FILE *csv_file = NULL;

static void handle_sigint(int sig)
{
    exiting = 1;
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
    return vfprintf(stderr, format, args);
}

// Converte __u32 (network byte order) em string dotted-decimal
static void ip_to_str(__u32 ip_be, char *buf, size_t buflen)
{
    struct in_addr addr = { .s_addr = ip_be };
    snprintf(buf, buflen, "%s", inet_ntoa(addr));
}

static void poll_and_dump(struct bpf_map *map)
{
    int map_fd = bpf_map__fd(map);
    struct flow_key key = {}, next_key;
    struct tcp_metrics metrics;
    char src_str[INET_ADDRSTRLEN], dst_str[INET_ADDRSTRLEN];
    time_t now = time(NULL);

    // bpf_map_get_next_key + lookup é o padrão para iterar um HASH map
    // inteiro a partir do user-space.
    while (bpf_map_get_next_key(map_fd, &key, &next_key) == 0) {
        if (bpf_map_lookup_elem(map_fd, &next_key, &metrics) == 0) {
            ip_to_str(next_key.src_ip, src_str, sizeof(src_str));
            ip_to_str(next_key.dst_ip, dst_str, sizeof(dst_str));

            printf("[%ld] %s:%d -> %s:%d | cwnd=%u ssthresh=%u srtt_us=%u "
                   "retrans=%u bytes_acked=%llu\n",
                   now, src_str, next_key.src_port, dst_str, next_key.dst_port,
                   metrics.snd_cwnd, metrics.ssthresh, metrics.srtt,
                   metrics.retransmissions, (unsigned long long)metrics.bytes_acked);

            if (csv_file) {
                fprintf(csv_file, "%ld,%s,%d,%s,%d,%u,%u,%u,%u,%llu\n",
                        now, src_str, next_key.src_port, dst_str, next_key.dst_port,
                        metrics.snd_cwnd, metrics.ssthresh, metrics.srtt,
                        metrics.retransmissions, (unsigned long long)metrics.bytes_acked);
                fflush(csv_file); // grava incrementalmente, útil se o processo for morto
            }
        }
        key = next_key;
    }
}

int main(int argc, char **argv)
{
    struct tcp_co_bpf *skel;
    int err;
    const char *csv_path = argc > 1 ? argv[1] : "tcp_co_output.csv";

    libbpf_set_print(libbpf_print_fn);

    skel = tcp_co_bpf__open_and_load();
    if (!skel) {
        fprintf(stderr, "Falha ao abrir/carregar o skeleton eBPF\n");
        return 1;
    }

    err = tcp_co_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "Falha ao anexar o programa eBPF: %d\n", err);
        goto cleanup;
    }

    csv_file = fopen(csv_path, "w");
    if (!csv_file) {
        fprintf(stderr, "Aviso: não foi possível abrir %s para escrita, seguindo sem CSV\n", csv_path);
    } else {
        fprintf(csv_file, "timestamp,src_ip,src_port,dst_ip,dst_port,snd_cwnd,ssthresh,srtt_us,retransmissions,bytes_acked\n");
    }

    printf("TCP-CO Etapa 2 rodando. Gravando em: %s\n", csv_path);
    printf("Pressione Ctrl+C para sair.\n\n");

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    // Polling a cada 1 segundo. Para os experimentos da Parte 4, o ideal
    // é diminuir para 100-200ms (mais granularidade no gráfico cwnd x tempo).
    while (!exiting) {
        poll_and_dump(skel->maps.tcp_flows);
        sleep(1);
    }

cleanup:
    if (csv_file)
        fclose(csv_file);
    tcp_co_bpf__destroy(skel);
    return err < 0 ? -err : 0;
}