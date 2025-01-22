/* ***************************************************
flvcast: flv streaming to rtmp
Greg Kennedy 2025

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

enum PlayMode {
	none,
	file,
	playlist,
	exec
};

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
	char * streamUrl;
	enum PlayMode playMode = none;
	char * playParam;
	unsigned int loops = 0;
	int verbose = 0, shuffle = 0;
	int opt;

	while ((opt = getopt(argc, argv, "vl:f:p:e:s")) != -1) {
		switch (opt) {
		case 'v':
			verbose = 1;
			break;

		case 'f':
			if (playMode != none) {
				fputs("ERROR: Arguments: Must use only one of -f, -p or -e\n", stderr);
				exit(EXIT_FAILURE);
			} else if (shuffle) {
				fputs("ERROR: Arguments: Cannot use -s without -p <playlist>\n", stderr);
				exit(EXIT_FAILURE);
			}

			playMode = file;
			playParam = optarg;
			break;

		case 'p':
			if (playMode != none) {
				fputs("ERROR: Arguments: Must use only one of -f, -p or -e\n", stderr);
				exit(EXIT_FAILURE);
			}

			playMode = playlist;
			playParam = optarg;
			break;

		case 'e':
			if (playMode != none) {
				fputs("ERROR: Arguments: Must use only one of -f, -p or -e\n", stderr);
				exit(EXIT_FAILURE);
			} else if (loops) {
				fputs("ERROR: Arguments: Cannot use -l <loops> with -e <script>\n", stderr);
				exit(EXIT_FAILURE);
			} else if (shuffle) {
				fputs("ERROR: Arguments: Cannot use -s without -p <playlist>\n", stderr);
				exit(EXIT_FAILURE);
			}

			playMode = exec;
			playParam = optarg;
			break;

		case 'l':
			if (playMode == exec) {
				fputs("ERROR: Arguments: Cannot use -l <loops> with -e <script>\n", stderr);
				exit(EXIT_FAILURE);
			}

			loops = atoi(optarg);
			break;

		case 's':
			if (playMode != playlist) {
				fputs("ERROR: Arguments: Cannot use -s without -p <playlist>\n", stderr);
				exit(EXIT_FAILURE);
			}

			shuffle = 1;
			break;

		default: /* '?' */
			fprintf(stderr, "Usage: %s (-f filename | -p playlist | -e executable) [-l loops] [-s] url\n", argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	// minimum required arguments
	if (playMode == none) {
		fputs("ERROR: Arguments: Must use one of -f, -p or -e\n", stderr);
		exit(EXIT_FAILURE);
	}

	// and get the Stream URL at the end
	if (optind != argc - 1) {
		fputs("ERROR: Arguments: Expected exactly one url argument after options\n", stderr);
		exit(EXIT_FAILURE);
	}

	streamUrl = argv[optind];
	// A "file" is really just a playlist of one.
	//  Set up the playlist by reading the file or creating this.
	/* *************************************************** */
	int ret = EXIT_SUCCESS;
	/* *************************************************** */
	// Increase the log level for all RTMP actions
	RTMP_LogSetLevel(RTMP_LOGINFO);
	RTMP_LogSetOutput(stderr);
	/* *************************************************** */
	// Init RTMP code
	RTMP * r = RTMP_Alloc();

	if (r == NULL) {
		perror("ERROR: RTMP_Alloc(): Failed to create RTMP object");
		exit(EXIT_FAILURE);
	}

	RTMP_Init(r);

	if (! RTMP_SetupURL(r, streamUrl)) {
		fprintf(stderr, "ERROR: RTMP_SetupURL(%s): Failed to setup stream URL\n", streamUrl);
		ret = EXIT_FAILURE;
		goto freeRTMP;
	}

	RTMP_EnableWrite(r);

	// Make RTMP connection to server
	if (! RTMP_Connect(r, NULL)) {
		fputs("ERROR: RTMP_Connect(): Failed to connect to remote RTMP server\n", stderr);
		ret = EXIT_FAILURE;
		goto freeRTMP;
	}

	// Connect to RTMP stream
	if (! RTMP_ConnectStream(r, 0)) {
		fputs("ERROR: RTMP_ConnectStream(): Failed to connect to RTMP stream\n", stderr);
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
	while (running) {
		// We need an FLV to begin with: there are several ways to get one
		FLV flv;

		switch (playMode) {
		case file:
			flv = flv_open(playParam);
			break;

		case playlist:
			break;
		}

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
					//if (DEBUG)
					//printf("Sleeping %lu milliseconds\n", timestamp - prevTimestamp);
					usleep(1000 * (timestamp - prevTimestamp));
					prevTimestamp = timestamp;
				}
			}
		}
	// Shut down
	flv_close(flv);
	}

	/* *************************************************** */
	// CLEANUP CODE
	// restore signal handlers
restoreSig:
	signal(SIGTERM, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
freeRTMP:
	RTMP_Free(r);
	return ret;
}
