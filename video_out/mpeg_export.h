/*-------------------------
Export code by
Adam Williams
broadcast@earthling.net
---------------------------*/





#ifndef MPEG_EXPORT_H
#define MPEG_EXPORT_H

#include "quicktime.h"

typedef struct {
	int export_it;           // 1 - export mode  0 - normal
	quicktime_t *video_file;    // file for quicktime export
	FILE *audio_file;
	char vid_compressor[4];   // compression method
	char aud_compressor[4];
	char video_path[1024], audio_path[1024];
	int jpeg_quality;
	unsigned char *raw_data;
	unsigned char **row_pointers;
	int w, h;
} exporter_t;

extern exporter_t export_data;

int exporter_init();
int exporter_end();
int exporter_set_path(char *path);
int exporter_set_vid_compressor(char *compressor);
int exporter_set_aud_compressor(char *compressor);
int exporter_frame();
int exporter_16to24_row(unsigned short *input, unsigned char *output, int w);
int exporter_32to24_row(unsigned char *input, unsigned char *output, int w);
int exporter_match_vid_compressor(char *test);
char* exporter_int_to_codec(int t);
unsigned char **exporter_get_rows(int w, int h);

#endif
