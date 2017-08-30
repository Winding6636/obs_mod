/******************************************************************************
Copyright (C) 2017 by c3r1c3 <c3r1c3@nevermindonline.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#include <util/bmem.h>
#include <util/c99defs.h>
#include <util/circlebuf.h>
#include <media-io/audio-io.h>

#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/samplefmt.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>

#include "obs-ffmpeg-audio-filters-compat.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OPTIONS_LENGTH                  1024
#define FILTER_NAME                       32
#define OMT_                            obs_module_text
#define warn(format, ...)  do_log(LOG_WARNING, format, ##__VA_ARGS__)
#define info(format, ...)  do_log(LOG_INFO,    format, ##__VA_ARGS__)
#define do_log(level, format, ...) \
	blog(level, "['%s': '%s'] " format, \
			, obs_source_get_name(gf->context), ##__VA_ARGS__)


struct fmpg_filter_settings {

	/* For the FFMPEG Setup: */
	AVFilterGraph                 *filter_graph;

	AVFilterContext               *abuffer_ctx;
	AVFilterContext               *aformat_ctx;
	AVFilterContext               *abuffersink_ctx;
	AVFilterContext               *audio_filter_ctx;

	AVFilter                      *audio_filter;
	AVFilter                      *abuffer_filter;
	AVFilter                      *aformat_filter;
	AVFilter                      *abuffersink;

	AVFilterInOut                 *out_filter;
	AVFilterInOut                 *in_filter;

	AVFrame                       *frame;
	AVCodecContext                *avc_ctx;
	AVRational                     time_base;

	char                           abuffer_str_opt[OPTIONS_LENGTH];
	char                           filter_str_opt[OPTIONS_LENGTH];
	char                           aformat_str_opt[OPTIONS_LENGTH];
	char                           filter_name[FILTER_NAME];
	char                           err_string[OPTIONS_LENGTH];
	float                          duration;
	int                            err_code;
	int                            nb_frames;
	int                            i;
	uint8_t                        fmpg_chan_layout[64];

	enum AVSampleFormat            obs_audio_format;
	enum AVSampleFormat            fmpg_audio_format;
	char                           fmpg_audio_format_str[OPTIONS_LENGTH];

	uint64_t                       fmpg_speaker_layout;
	char                           fmpg_speaker_layout_str[OPTIONS_LENGTH];

	size_t                         obs_chan_count;
	uint32_t                       obs_sample_rate;
	size_t                         obs_frame_size; //AUDIO_OUTPUT_FRAMES;
};

typedef struct ffmpeg_filter_settings ffmpeg_filter_settings_t;

/*
 * Here is the common code to build a filter/scene graph to use the various
 * FFMPEG audio plug-ins. Before this function call one should construct the
 * FFMPEG audio filter and pass those values into the
 * ffmpeg_filter_settings struct so the filter can be properly constructed.
 */
//extern void ffmpeg_filter_graph_create(void *ffmpeg_settings);

//extern void ffmpeg_filter_graph_update(void *ffmpeg_settings);

//extern void ffmpeg_filter_process(void *ffmpeg_settings);

/*
 * The below comes from audio-resampler-ffmpeg.c and if the
 * functions in that file ever get converted into 'public'
 * functions, convert to that and take this out.
 */

static enum AVSampleFormat convert_audio_format(enum audio_format format)
{
	switch (format) {
		case AUDIO_FORMAT_UNKNOWN:      return AV_SAMPLE_FMT_S16;
		case AUDIO_FORMAT_U8BIT:        return AV_SAMPLE_FMT_U8;
		case AUDIO_FORMAT_16BIT:        return AV_SAMPLE_FMT_S16;
		case AUDIO_FORMAT_32BIT:        return AV_SAMPLE_FMT_S32;
		case AUDIO_FORMAT_FLOAT:        return AV_SAMPLE_FMT_FLT;
		case AUDIO_FORMAT_U8BIT_PLANAR: return AV_SAMPLE_FMT_U8P;
		case AUDIO_FORMAT_16BIT_PLANAR: return AV_SAMPLE_FMT_S16P;
		case AUDIO_FORMAT_32BIT_PLANAR: return AV_SAMPLE_FMT_S32P;
		case AUDIO_FORMAT_FLOAT_PLANAR: return AV_SAMPLE_FMT_FLTP;
	}

	/* shouldn't get here */
	return AV_SAMPLE_FMT_S16;
}

/*
 * The below comes from audio-resampler-ffmpeg.c and if the
 * functions in that file ever get converted into 'public'
 * functions, convert to that and take this out.
 */
static uint64_t convert_speaker_layout(enum speaker_layout layout)
{
	switch (layout) {
		case SPEAKERS_UNKNOWN:          return 0;
		case SPEAKERS_MONO:             return AV_CH_LAYOUT_MONO;
		case SPEAKERS_STEREO:           return AV_CH_LAYOUT_STEREO;
		case SPEAKERS_2POINT1:          return AV_CH_LAYOUT_2_1;
		case SPEAKERS_QUAD:             return AV_CH_LAYOUT_QUAD;
		case SPEAKERS_4POINT1:          return AV_CH_LAYOUT_4POINT1;
		case SPEAKERS_5POINT1:          return AV_CH_LAYOUT_5POINT1;
		case SPEAKERS_5POINT1_SURROUND: return AV_CH_LAYOUT_5POINT1_BACK;
		case SPEAKERS_7POINT1:          return AV_CH_LAYOUT_7POINT1;
		case SPEAKERS_7POINT1_SURROUND: return AV_CH_LAYOUT_7POINT1_WIDE_BACK;
		case SPEAKERS_SURROUND:         return AV_CH_LAYOUT_SURROUND;
	}

	/* shouldn't get here */
	return 0;
}

/* Now we have to convert the enums into usable strings. */
static char *convert_audio_format_string(enum audio_format format)
{
	switch (format) {
		case AUDIO_FORMAT_UNKNOWN:      return "AV_SAMPLE_FMT_S16\0";
		case AUDIO_FORMAT_U8BIT:        return "AV_SAMPLE_FMT_U8\0";
		case AUDIO_FORMAT_16BIT:        return "AV_SAMPLE_FMT_S16\0";
		case AUDIO_FORMAT_32BIT:        return "AV_SAMPLE_FMT_S32\0";
		case AUDIO_FORMAT_FLOAT:        return "AV_SAMPLE_FMT_FLT\0";
		case AUDIO_FORMAT_U8BIT_PLANAR: return "AV_SAMPLE_FMT_U8P\0";
		case AUDIO_FORMAT_16BIT_PLANAR: return "AV_SAMPLE_FMT_S16P\0";
		case AUDIO_FORMAT_32BIT_PLANAR: return "AV_SAMPLE_FMT_S32P\0";
		case AUDIO_FORMAT_FLOAT_PLANAR: return "AV_SAMPLE_FMT_FLTP\0";
	}

	/* shouldn't get here */
	return "AV_SAMPLE_FMT_FLT\0";
}

static char *convert_speaker_layout_string(enum speaker_layout layout)
{
	switch (layout) {
		case SPEAKERS_UNKNOWN:          return "0\0";
		case SPEAKERS_MONO:             return "AV_CH_LAYOUT_MONO\0";
		case SPEAKERS_STEREO:           return "AV_CH_LAYOUT_STEREO\0";
		case SPEAKERS_2POINT1:          return "AV_CH_LAYOUT_2_1\0";
		case SPEAKERS_QUAD:             return "AV_CH_LAYOUT_QUAD\0";
		case SPEAKERS_4POINT1:          return "AV_CH_LAYOUT_4POINT1\0";
		case SPEAKERS_5POINT1:          return "AV_CH_LAYOUT_5POINT1\0";
		case SPEAKERS_5POINT1_SURROUND: return "AV_CH_LAYOUT_5POINT1_BACK\0";
		case SPEAKERS_7POINT1:          return "AV_CH_LAYOUT_7POINT1\0";
		case SPEAKERS_7POINT1_SURROUND: return "AV_CH_LAYOUT_7POINT1_WIDE_BACK\0";
		case SPEAKERS_SURROUND:         return "AV_CH_LAYOUT_SURROUND\0";
	}

	/* shouldn't get here */
	return "0\0";
}

#ifdef __cplusplus
}
#endif
