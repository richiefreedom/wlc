FILES = wlc.c

all:
	gcc -O2 -o wlc $(FILES) lib/mpc/mpc.c -lm

clean:
	rm -f wlc *.o
