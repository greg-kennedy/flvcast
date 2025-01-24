#ifndef FLV_H_
#define FLV_H_

typedef struct flv * FLV;

// open an FLV from a file
FLV flv_open(const char * filename, int verbose);

// advance to next frame in FLV
//  updates tag data and returns the new tag_size
//  0 if EOF, <0 if error
long flv_next(FLV f);

// tag and tag size accessors
const unsigned char * flv_get_tag(FLV f);
unsigned long flv_get_tag_size(FLV f);

// get / set timestamp on FLV frame
unsigned long flv_get_timestamp(FLV f);
void flv_set_timestamp(FLV f, unsigned long timestamp);

// close FLV
void flv_close(FLV flv);

#endif
