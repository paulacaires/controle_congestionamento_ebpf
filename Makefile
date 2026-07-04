CLANG ?= clang
BPFTOOL ?= /usr/sbin/bpftool
ARCH := $(shell uname -m | sed 's/x86_64/x86/' | sed 's/aarch64/arm64/')

CFLAGS_BPF := -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) -I.
CFLAGS_USER := -g -O2 -I.

.PHONY: all clean vmlinux

all: tcp_co

# 1. Gera o vmlinux.h a partir do BTF do kernel rodando (precisa existir
#    /sys/kernel/btf/vmlinux — normal em kernels 5.2+ com CONFIG_DEBUG_INFO_BTF=y)
vmlinux.h:
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h

# 2. Compila o objeto eBPF
tcp_co.bpf.o: tcp_co.bpf.c vmlinux.h
	$(CLANG) $(CFLAGS_BPF) -c tcp_co.bpf.c -o tcp_co.bpf.o

# 3. Gera o skeleton (header com structs/funções para o loader usar)
tcp_co.skel.h: tcp_co.bpf.o
	$(BPFTOOL) gen skeleton tcp_co.bpf.o > tcp_co.skel.h

# 4. Compila o loader user-space, já linkando com libbpf
tcp_co: tcp_co.c tcp_co.skel.h
	$(CLANG) $(CFLAGS_USER) tcp_co.c -o tcp_co -lbpf -lelf -lz

clean:
	rm -f tcp_co tcp_co.bpf.o tcp_co.skel.h vmlinux.h