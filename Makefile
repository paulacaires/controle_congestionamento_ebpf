CLANG ?= clang
BPFTOOL ?= bpftool
ARCH := $(shell uname -m | sed 's/x86_64/x86/' | sed 's/aarch64/arm64/')

CFLAGS_BPF := -g -O2 -target bpf -D__TARGET_ARCH_$(ARCH) -I.
CFLAGS_USER := -g -O2 -I.

.PHONY: all clean

all: tcp_co

vmlinux.h:
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h

tcp_co.bpf.o: tcp_co.bpf.c vmlinux.h
	$(CLANG) $(CFLAGS_BPF) -c tcp_co.bpf.c -o tcp_co.bpf.o

tcp_co.skel.h: tcp_co.bpf.o
	$(BPFTOOL) gen skeleton tcp_co.bpf.o > tcp_co.skel.h

tcp_co: tcp_co.c tcp_co.skel.h
	$(CLANG) $(CFLAGS_USER) tcp_co.c -o tcp_co -lbpf -lelf -lz

clean:
	rm -f tcp_co tcp_co.bpf.o tcp_co.skel.h vmlinux.h tcp_co_output.csv