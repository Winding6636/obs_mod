/******************************************************************************
    Copyright (C) 2013 by Hugh Bailey <obs.jim@gmail.com>

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

#include "../util/bmem.h"
#include "audio-panner.h"
#include "audio-io.h"
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>

struct audio_panner {
	struct PanContext              *context;
	bool                            opened;

	uint32_t                        input_freq;
	uint64_t                        input_layout;
	enum AVSampleFormat             input_format;
	struct AVFilter                 panning_filter;

	uint8_t                        *output_buffer[MAX_AV_PLANES];
	uint64_t                        output_layout;
	enum AVSampleFormat             output_format;
	int                             output_size;
	uint32_t                        output_ch;
	uint32_t                        output_freq;
	uint32_t                        output_planes;
};

static inline enum AVSampleFormat convert_audio_format(enum audio_format format)
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

static inline uint64_t convert_speaker_layout(enum speaker_layout layout)
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

audio_panner_t *audio_panner_create(const struct pan_info *dst,
		const struct pan_info *src)
{
	struct audio_panner *pan = bzalloc(sizeof(struct audio_panner));
	int errcode;

	pan->opened        = false;
	pan->input_freq    = src->samples_per_sec;
	pan->input_layout  = convert_speaker_layout(src->speakers);
	pan->input_format  = convert_audio_format(src->format);
	pan->output_size   = 0;
	pan->output_ch     = get_audio_channels(dst->speakers);
	pan->output_freq   = dst->samples_per_sec;
	pan->output_layout = convert_speaker_layout(dst->speakers);
	pan->output_format = convert_audio_format(dst->format);
	pan->output_planes = is_audio_planar(dst->format) ? pan->output_ch : 1;

	pan->context = swr_alloc_set_opts(NULL,
		pan->output_layout, pan->output_format, dst->samples_per_sec,
		pan->input_layout,  pan->input_format,  src->samples_per_sec,
		0, NULL);

	if (!pan->context) {
		blog(LOG_ERROR, "swr_alloc_set_opts failed");
		audio_panner_destroy(pan);
		return NULL;
	}

	errcode = swr_init(pan->context);
	if (errcode != 0) {
		blog(LOG_ERROR, "avresample_open failed: error code %d",
				errcode);
		audio_panner_destroy(pan);
		return NULL;
	}

	return pan;
}

void audio_panner_destroy(audio_panner_t *pan)
{
	if (pan) {
		if (pan->context)
			swr_free(&pan->context);
		if (pan->output_buffer[0])
			av_freep(&pan->output_buffer[0]);

		bfree(pan);
	}
}

bool audio_panner_pan(audio_panner_t *pan,
		 uint8_t *output[], uint32_t *out_frames, uint64_t *ts_offset,
		 const uint8_t *const input[], uint32_t in_frames)
{
	if (!pan) return false;

	struct SwrContext *context = pan->context;
	int ret;

	int64_t delay = swr_get_delay(context, pan->input_freq);
	int estimated = (int)av_rescale_rnd(
			delay + (int64_t)in_frames,
			(int64_t)pan->output_freq, (int64_t)pan->input_freq,
			AV_ROUND_UP);

	*ts_offset = (uint64_t)swr_get_delay(context, 1000000000);

	/* resize the buffer if bigger */
	if (estimated > pan->output_size) {
		if (pan->output_buffer[0])
			av_freep(&pan->output_buffer[0]);

		av_samples_alloc(pan->output_buffer, NULL, pan->output_ch,
				estimated, pan->output_format, 0);

		pan->output_size = estimated;
	}

	ret = swr_convert(context,
			pan->output_buffer, pan->output_size,
			(const uint8_t**)input, in_frames);

	if (ret < 0) {
		blog(LOG_ERROR, "swr_convert failed: %d", ret);
		return false;
	}

	for (uint32_t i = 0; i < pan->output_planes; i++)
		output[i] = pan->output_buffer[i];

	*out_frames = (uint32_t)ret;
	return true;
}
