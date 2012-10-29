CFLAGS = -g -Wall

all: exncall32o0 exncall64o0 exncall32o3 exncall64o3

exncall32o0: exncall.c
	gcc $(CFLAGS) $^ -o $@ -m32 -O0
exncall64o0: exncall.c
	gcc $(CFLAGS) $^ -o $@ -m64 -O0
exncall32o3: exncall.c
	gcc $(CFLAGS) $^ -o $@ -m32 -O3
exncall64o3: exncall.c
	gcc $(CFLAGS) $^ -o $@ -m64 -O3
