/* ***************************************************
rtmpcast: librtmp example code
Greg Kennedy 2021

Sends an input FLV file to a designated RTMP URL.
*************************************************** */
#include "flv.h"

#include <librtmp/rtmp.h>
#include <librtmp/log.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#define DEBUG 0

// Flag to indicate whether we should keep playing the movie
//  Set to 0 to close the program
static int running;
// replacement signal handler that sets running to 0 for clean shutdown
static void sig_handler(int signum)
{
	fprintf(stderr, "Received signal %d (%s), exiting.\n", signum, strsignal(signum));
	running = 0;
}

/* *************************************************** */
int main(int argc, char * argv[])
{
	int ret = EXIT_SUCCESS;

	// verify two parameters passed
	if (argc != 3) {
		printf("RTMP example code\nUsage:\n\t%s <INPUT.FLV> <URL>\n", argv[0]);
		goto exit;
	}

	// Let's open an FLV now
	FLV flv = flv_open(argv[1]);

	/* *************************************************** */
	// Increase the log level for all RTMP actions
	RTMP_LogSetLevel(RTMP_LOGINFO);
	RTMP_LogSetOutput(stderr);

	/* *************************************************** */
	// Init RTMP code
	RTMP * r = RTMP_Alloc();
	RTMP_Init(r);

	if (r == NULL) {
		fputs("Failed to create RTMP object\n", stderr);
		ret = EXIT_FAILURE;
		goto closeFLV;
	}

	RTMP_SetupURL(r, argv[2]);
	RTMP_EnableWrite(r);

	// Make RTMP connection to server
	if (! RTMP_Connect(r, NULL)) {
		fputs("Failed to connect to remote RTMP server\n", stderr);
		ret = EXIT_FAILURE;
		goto freeRTMP;
	}

	// Connect to RTMP stream
	if (! RTMP_ConnectStream(r, 0)) {
		fputs("Failed to connect to RTMP stream\n", stderr);
		ret = EXIT_FAILURE;
		goto freeRTMP;
	}

	// track the fd for rtmp
	int fd = RTMP_Socket(r);
	struct timeval tv = {0, 0};

	// Let's install some signal handlers for a graceful exit
	running = 1;
	signal(SIGTERM, sig_handler);
	signal(SIGINT, sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGHUP, sig_handler);

	/* *************************************************** */
	// Ready to start throwing frames at the streamer
	const unsigned char * tag = flv_get_tag(flv);
	unsigned long prevTimestamp = 0;

	while (running) {
		// read current block
		long tagSize = flv_next(flv);
		if (tagSize == 0)
			running = 0;
		else if (tagSize < 0) {
			ret = EXIT_FAILURE;
			goto restoreSig;
		} else {
			// Toss into RTMP
			//  cast to char* avoids a warning
			if (RTMP_Write(r, tag, tagSize) <= 0) {
				fputs("Failed to RTMP_Write\n", stderr);
				ret = EXIT_FAILURE;
				goto restoreSig;
			}

			// Handle any packets from the remote to us.
			//  We will use select() to see if packet is waiting,
			//  then read it and dispatch to the handler.
			fd_set set;
			FD_ZERO(&set);
			FD_SET(fd, &set);

			if (select(fd + 1, &set, NULL, NULL, &tv) == -1) {
				perror("Error calling select()");
				ret = EXIT_FAILURE;
				goto restoreSig;
			}

			// socket is present in read-ready set, safe to call RTMP_ReadPacket
			if (FD_ISSET(fd, &set)) {
				RTMPPacket packet = { 0 };

				if (RTMP_ReadPacket(r, &packet) && RTMPPacket_IsReady(&packet)) {
					// this function does all the internal stuff we need
					RTMP_ClientPacket(r, &packet);
					RTMPPacket_Free(&packet);
				}
			}

			// delay
			//  this is to avoid sending too many frames to the RTMP server,
			//  overwhelming it.  this isn't the most accurate way to do it
			//  but with server buffering it works OK.
			unsigned long timestamp = flv_get_timestamp(flv);
			if (prevTimestamp < timestamp) {
				if (DEBUG)
					printf("Sleeping %lu milliseconds\n", timestamp - prevTimestamp);

				usleep(1000 * (timestamp - prevTimestamp));
				prevTimestamp = timestamp;
			}
		}
	}

	/* *************************************************** */
	// CLEANUP CODE
	// restore signal handlers
restoreSig:
	signal(SIGTERM, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	// Shut down
freeRTMP:
	RTMP_Free(r);
closeFLV:
	flv_close(flv);
exit:
	return ret;
}
