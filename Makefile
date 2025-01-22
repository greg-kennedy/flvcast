CFLAGS += -Wall -Wextra -I/usr/local/include -I/home/grkenn/src/rtmpdump  -L/usr/local/lib -L/home/grkenn/src/rtmpdump/librtmp
OFLAGS += -Ofast -march=native -flto
DFLAGS += -g -fsanitize=address,undefined,leak,integer
IFLAGS += -I/usr/local/include
LFLAGS += -L/usr/local/lib

.SUFFIXES:

all:	flvcast flvcast-safe

flvcast:	flv.c main.c
	cc $(IFLAGS) $(CFLAGS) $(OFLAGS) $(LFLAGS) -o flvcast flv.c main.c -lrtmp

flvcast-safe:	flv.c main.c
	cc $(IFLAGS) $(CFLAGS) $(DFLAGS) $(LFLAGS) -o flvcast-safe flv.c main.c -lrtmp

clean:
	rm -f flvcast flvcast-safe *.o
