#include "flv.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <errno.h>

#define FLV_FILE_HEADER_SIZE 9ul

// maximum size of a tag is 11 byte header, 0xFFFFFF payload, 4 byte size
#define FLV_TAG_HEADER_SIZE 11ul
#define FLV_TAG_PAYLOAD_MAX_SIZE 0xFFFFFFul
#define FLV_TAG_FOOTER_SIZE 4ul

#define FLV_MAX_TAG_SIZE (FLV_TAG_HEADER_SIZE + FLV_TAG_PAYLOAD_MAX_SIZE + FLV_TAG_FOOTER_SIZE)

struct flv {
	FILE * file;
	unsigned char tag[FLV_MAX_TAG_SIZE];
	unsigned long tagSize;
};

// helper functions
//  parse 24 bits to an unsigned long
static unsigned long u24be(const unsigned char * const p)
{
	return *p << 16 | *(p + 1) << 8 | *(p + 2);
}

//  parse 32 bits to an unsigned long
static unsigned long u32be(const unsigned char * const p)
{
	return *p << 24 | *(p + 1) << 16 | *(p + 2) << 8 | *(p + 3);
}

struct flv * flv_open(const char * filename)
{
	/* *************************************************** */
	// allocate a very large buffer for all packets and operations
	struct flv * f = malloc(sizeof(struct flv));

	if (f == NULL) {
		fprintf(stderr, "ERROR: flv_open(%s): malloc(%lu): %s\n", filename, sizeof(struct flv), strerror(errno));
		goto returnError;
	}

	/* *************************************************** */
	// Let's open an FLV now
	f->file = fopen(filename, "rb");

	if (f->file == NULL) {
		fprintf(stderr, "ERROR: flv_open(%s): fopen(%s): %s\n", filename, filename, strerror(errno));
		goto freeFLV;
	}

	// make sure it's supported FLV by reading the FLV Header (9 bytes)
	if (1 != fread(f->tag, FLV_FILE_HEADER_SIZE, 1, f->file)) {
		fprintf(stderr, "ERROR: flv_open(%s): fread(%lu): %s\n", filename, FLV_FILE_HEADER_SIZE, strerror(errno));
		goto closeFile;
	}

	// 'F', 'L', 'V', 0x01
	unsigned long flvHeader = u32be(f->tag);

	if (flvHeader != 0x464C5601) {
		fprintf(stderr, "ERROR: flv_open(%s): header: Does not appear to be valid FLV1 file (got %08lx)\n", filename, flvHeader);
		goto closeFile;
	}

	/*
		if (f->tag[4] & 0x01)
			puts("FLV contains VIDEO");

		if (f->tag[4] & 0x04)
			puts("FLV contains AUDIO");
	*/

	if ((f->tag[4] & 0x05) == 0)
		fprintf(stderr, "Warning: flv_open(%s): FLV header byte (%02x) does not indicate VIDEO nor AUDIO?\n", filename, f->tag[4]);

	unsigned long flvStartTag = u32be(f->tag + 5);

	/* *************************************************** */
	if (flvStartTag != FLV_FILE_HEADER_SIZE) {
		fprintf(stderr, "Warning: flv_open(%s): flvStartTag expected %lu, got %lu\n", filename, FLV_FILE_HEADER_SIZE, flvStartTag);

		if (0 != fseek(f->file, flvStartTag, SEEK_SET)) {
			fprintf(stderr, "ERROR: flv_open(%s): fseek(%lu): %s\n", filename, flvStartTag, strerror(errno));
			goto closeFile;
		}
	}

	if (1 != fread(f->tag, FLV_TAG_FOOTER_SIZE, 1, f->file)) {
		fprintf(stderr, "ERROR: flv_open(%s): fread(%lu): %s\n", filename, FLV_TAG_FOOTER_SIZE, strerror(errno));
		goto closeFile;
	}

	unsigned long flvTagSize0 = u32be(f->tag);

	if (flvTagSize0 != 0)
		fprintf(stderr, "Warning: flv_open(%s): flvTagSize0 expected 0, got %lu\n", filename, flvTagSize0);

	return f;
closeFile:
	fclose(f->file);
freeFLV:
	free(f);
returnError:
	return NULL;
}

long flv_next(struct flv * f)
{
	// read current block header
	if (1 != fread(f->tag, FLV_TAG_HEADER_SIZE, 1, f->file)) {
		// failed to read next tag - probably end-of-file.
		if (feof(f->file))
			return 0;
		else {
			fprintf(stderr, "ERROR: flv_next(%p): fread(%lu): %s\n", f, FLV_TAG_HEADER_SIZE, strerror(errno));
			return -1;
		}
	}

	// Successfully got header.  Parse it.
	//unsigned char payloadType = f->tag[0];
	unsigned long payloadSize = u24be(f->tag + 1);
	//unsigned long timestamp = u24be(f->tag + 4) | (f->tag[7] << 24);

	//unsigned long streamId = u24be(f->tag + 8);

	//if (DEBUG)
	//	printf("Position %lu, Type %hhu, Size %lu, Timestamp %lu, Stream %lu\n", ftell(flv), payloadType, payloadSize, timestamp, streamId);

	// Read the rest of the payload.
	if (1 != fread(f->tag + FLV_TAG_HEADER_SIZE, payloadSize + FLV_TAG_FOOTER_SIZE, 1, f->file)) {
		fprintf(stderr, "ERROR: flv_next(%p): fread(%lu): %s\n", f, payloadSize + FLV_TAG_FOOTER_SIZE, strerror(errno));
		return -1;
	}

	// Double-check that we got our payload size right
	unsigned long storedPayloadSize = u32be(f->tag + (FLV_TAG_HEADER_SIZE + payloadSize));

	if (storedPayloadSize != FLV_TAG_HEADER_SIZE + payloadSize) {
		fprintf(stderr, "ERROR: flv_next(%p): Read tag size %lu does not match calculated tag size %lu\n", f, storedPayloadSize, FLV_TAG_HEADER_SIZE + payloadSize);
		return -1;
	}

	// set the overall tag size
	f->tagSize = FLV_TAG_HEADER_SIZE + payloadSize + FLV_TAG_FOOTER_SIZE;
	return f->tagSize;
}

const unsigned char * flv_get_tag(struct flv * f)
{
	return f->tag;
}

unsigned long flv_get_tag_size(struct flv * f)
{
	return f->tagSize;
}

unsigned long flv_get_timestamp(struct flv * f)
{
	return u24be(f->tag + 4) | (*(f->tag + 7) << 24);
}

void flv_set_timestamp(struct flv * f, unsigned long timestamp)
{
	f->tag[4] = (timestamp >> 16) & 0xFF;
	f->tag[5] = (timestamp >> 8) & 0xFF;
	f->tag[6] = timestamp & 0xFF;
	f->tag[7] = (timestamp >> 24) & 0xFF;
}

void flv_close(struct flv * f)
{
	fclose(f->file);
	free(f);
}
