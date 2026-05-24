# H100 iron probes — requires CUDA toolkit (libcuda)
PROGS = iron_probe_v1 iron_gate_v1
CUDA_HOME ?= /usr/local/cuda
CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -O2 -I$(CUDA_HOME)/include
LDFLAGS ?= -L$(CUDA_HOME)/lib64 -L$(CUDA_HOME)/targets/x86_64-linux/lib -lcuda -lcublas -ldl -lm

.PHONY: all clean

all: $(PROGS)

iron_probe_v1: iron_probe_v1.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

iron_gate_v1: iron_gate_v1.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(PROGS)
