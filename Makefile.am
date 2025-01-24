AM_CFLAGS = -Wall -Wextra

bin_PROGRAMS = flvcast

flvcast_SOURCES = main.c \
		flv.c
flvcast_CFLAGS = $(RTMP_CFLAGS) -fsanitize=address,undefined,leak,integer
flvcast_LDFLAGS = $(RTMP_LIBS)
