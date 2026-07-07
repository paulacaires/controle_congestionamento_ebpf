# Análise avançada de congestionamento TCP usando eBPF
Estou usando o Debian!

```bash
sudo apt update

sudo apt install -y \
    clang \
    llvm \
    libbpf-dev \
    bpftool \
    build-essential

sudo apt install bpftool
```

Executar o comando, para confirmar que o BTF do kernel está disponível.
```bash
ls -lh /sys/kernel/btf/vmlinux

ls -la /sys/kernel/btf/vmlinux
```
Tem que retornar: -r--r--r-- ... /sys/kernel/btf/vmlinux

Pra conferir as versões:
```bash
bpftool version
clang --version
pkg-config --modversion libbpf
make clean
make
```

### Fentry

Existem ferramentas que permitem rastrear funções do kernel. Vi duas: o Fentry e o Kprobe.

Kprobe significa Kernel Probe (probe = sonda). Com ele, você pode attach code snippets em localizações específicas do kernel (quase todas as instruções) do kernel.

Fonte: [Discovering the Magic of Kprobe: A Fun Introduction to Kernel Probing](https://medium.com/@giorgiodevops/discovering-the-magic-of-kprobe-a-fun-introduction-to-kernel-probing-b18d81760703)

Existe uma alternativa mais moderna ao Kprobe: O Fentry. O "problema" é que você só consegue colocá-lo na entrada de uma função do kernel, enquanto o Kprobe você pode anexar a sua sonda em qualquer instrução, dentro ou na entrada de uma função do kernel. Por outro lado, o Fentry é mais otimizado e fácil de utilizar.
Outro problema: nem toda função do kernel pode receber um fentry, então precisa verificar. Primeiro, a função precisa existir no BTF do kernel e estar disponível para attachment.
Para validar isso:
```bash
sudo bpftool btf dump file /sys/kernel/btf/vmlinux | grep tcp_rcv_established
```
Se aparecer "FUNC 'tcp_rcv_established' então está funcionando."

- [Exemplo de como utilizar o Kprobe](https://eunomia.dev/tutorials/2-kprobe-unlink/)
- [Exemplo de como utilizar o Fentry](https://eunomia.dev/tutorials/3-fentry-unlink/)
- [Outra fonte boa que relaciona Fentry com eBPF](https://bootlin.com/blog/bouncing-on-trampolines-to-run-ebpf-programs/)

## Primeira etapa: Hook eBPF mínimo
Dispara a imprime `snd_cwnd` via `bpf_printk`. O hook escolhido é o `tcp_rcv_established`, disparado toda vez que um pacote chega numa conexão estabelecida (quando chegam segmentos em uma conexão estabelecida).
Daí do hook você lê a `struct tcp_sock *tp` e pega `tp->snd_cwnd`, `tp->snd_ssthresh`, `tp->srtt_us >> 3` via BPF_CORE_READ.
Escolher uma função que é sempre chamada quando acontece o que eu quero mapear.
A variável "total_retrans" é atualizada em outos lugares (depois verificar onde eu vou pegar ela, mas eu vi que é na função `tcp_retransmit_skb()`).

> linux-headers: Quando eu digito no terminal `linux-headers-$(uname -r)` eu instalo milhares de arquivos .h que descrevem as estruturas do kernel. Dessa maneira, programas externos conseguem saber como o kernel é organizado (O problema é o CO-RE que na teoria não deveria ser tão específico em relação à estrutura do kernel, por isso existe o vmlinux.h)

### Arquivo `vmlinux.h`
Arquivo com definições da estrutura do kernel.

```bash
ls -lh /sys/kernel/btf/vmlinux
bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h
```

## Segunda etapa: Mapa por conexão
Implementar a `struct flow_key` e `struct tcp_metrics` exatamente como `BPF_MAP_TYPE_HASH`.


