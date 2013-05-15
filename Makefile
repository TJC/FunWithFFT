CFLAGS=-O2 -Wall -std=gnu99
# CFLAGS=-ggdb -Wall

all: capture

spi.o: spi.c spi.h Makefile
	gcc ${CFLAGS} -o spi.o -c spi.c

alsa.o: alsa.c alsa.h Makefile
	gcc ${CFLAGS} -o alsa.o -c alsa.c

capture: capture.c Makefile spi.o alsa.o
	gcc ${CFLAGS} -o capture capture.c spi.o alsa.o -lasound -l fftw3f -lm

clean:
	-rm spi.o alsa.o capture.o capture
