/*-------------------------
Export code by
Adam Williams
broadcast@earthling.net
---------------------------*/





#include <stdio.h>
#include "mpeg_export.h"

exporter_t export_data;

int exporter_init()
{
	export_data.export_it = 0;
	export_data.video_file = 0;
	export_data.audio_file = 0;
	export_data.raw_data = 0;
	export_data.jpeg_quality = 100;
	export_data.row_pointers = 0;
	strcpy(export_data.video_path, "video.mov");
	strcpy(export_data.audio_path, "audio.pcm");
	exporter_set_vid_compressor(QUICKTIME_YUV4);
	exporter_set_aud_compressor(QUICKTIME_TWOS);
}

int exporter_end()
{
	if(export_data.raw_data) free(export_data.raw_data);
	if(export_data.video_file) quicktime_close(export_data.video_file);
	if(export_data.audio_file) fclose(export_data.audio_file);
	if(export_data.row_pointers) free(export_data.row_pointers);
}

int exporter_set_path(char *path)
{
	strcpy(export_data.video_path, path);
}

int exporter_set_vid_compressor(char *compressor)
{
	export_data.vid_compressor[0] = compressor[0];
	export_data.vid_compressor[1] = compressor[1];
	export_data.vid_compressor[2] = compressor[2];
	export_data.vid_compressor[3] = compressor[3];
}

int exporter_match_vid_compressor(char *test)
{
	if(test[0] == export_data.vid_compressor[0] &&
	test[1] == export_data.vid_compressor[1] &&
	test[2] == export_data.vid_compressor[2] &&
	test[3] == export_data.vid_compressor[3])
		return 1;
	else
		return 0;
}

char* exporter_int_to_codec(int t)
{
	switch(t)
	{
		case 0:
			return QUICKTIME_RAW;
			break;
		case 1:
			return QUICKTIME_JPEG;
			break;
		case 2:
			return QUICKTIME_YUV2;
			break;
		case 3:
			return QUICKTIME_YUV4;
			break;
		default:
			return QUICKTIME_RAW;
			break;
	}
}

int exporter_set_aud_compressor(char *compressor)
{
	export_data.aud_compressor[0] = compressor[0];
	export_data.aud_compressor[1] = compressor[1];
	export_data.aud_compressor[2] = compressor[2];
	export_data.aud_compressor[3] = compressor[3];
}

unsigned char **exporter_get_rows(int w, int h)
{
	int i;
	
/* get row pointers for the output buffer */
	if(!export_data.raw_data)
		export_data.raw_data = (unsigned char*)malloc(export_data.w * 3 * export_data.h);

	if(!export_data.row_pointers)
	{
		export_data.row_pointers = (unsigned char**)malloc(sizeof(unsigned char*) * export_data.h);
		for(i = 0; i < export_data.h; i++)
		{
			export_data.row_pointers[i] = &(export_data.raw_data[i * export_data.w * 3]);
		}
	}
	return export_data.row_pointers;
}


int exporter_frame()
{
	quicktime_encode_video(export_data.video_file, export_data.row_pointers, 0);
}

int exporter_16to24_row(unsigned short *input, unsigned char *output, int w)
{
	unsigned short *input_end = input + w;

	while(input < input_end)
	{
		*output++ = (*input & 0xf800) >> 8;
		*output++ = (*input & 0x7e0) >> 3;
		*output++ = (*input & 0x1f) << 3;
		input++;
	}
}

int exporter_32to24_row(unsigned char *input, unsigned char *output, int w)
{
	unsigned char *input_end = input + w * 4;

	while(input < input_end)
	{
// Big endian input
		input++;
		*output++ = *input++;
		*output++ = *input++;
		*output++ = *input++;
	}
}
