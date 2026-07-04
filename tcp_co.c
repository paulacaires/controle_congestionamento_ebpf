#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <bpf/libbpf.h>
#include "tcp_co.skel.h"

static volatile sig_atomic_t exiting = 0;

static void handle_sigint(int sig)
{
    exiting = 1;
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
    return vfprintf(stderr, format, args);
}

int main(int argc, char **argv)
{
    struct tcp_co_bpf *skel;
    int err;

    libbpf_set_print(libbpf_print_fn);

    // Abre, carrega e verifica o programa eBPF (skeleton gerado pelo bpftool)
    skel = tcp_co_bpf__open_and_load();
    if (!skel) {
        fprintf(stderr, "Falha ao abrir/carregar o skeleton eBPF\n");
        return 1;
    }

    // Anexa o fentry ao kernel
    /*
        "Quando alguém entrar em tcp_rcv_established(),
        execute também este programa."
    */
    err = tcp_co_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "Falha ao anexar o programa eBPF: %d\n", err);
        goto cleanup;
    }

    printf("TCP-CO Etapa 1 rodando. Pressione Ctrl+C para sair.\n");
    printf("Para ver os eventos em outro terminal:\n");
    printf("  sudo cat /sys/kernel/debug/tracing/trace_pipe\n");

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    while (!exiting) {
        pause();
    }

cleanup:
    tcp_co_bpf__destroy(skel);
    return err < 0 ? -err : 0;
}