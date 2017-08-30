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

#include <obs-module.h>
#include "headers/obs-ffmpeg-audio-filters.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-ffmpeg-audio-filters", "en-US")

#define SETTING_I_GAIN                 "input_gain"
#define SETTING_THRESHOLD              "threshold"
#define SETTING_RATIO                  "ratio"
#define SETTING_ATTACK                 "attack"
#define SETTING_RELEASE                "release"
#define SETTING_M_GAIN                 "makeup_gain"
#define SETTING_KNEE                   "knee"
#define SETTING_LINK                   "link"
#define SETTING_DETECTION              "detection"
#define SETTING_MIX                    "mix"

#define TEXT_I_GAIN                    OMT_("Input.Gain")
#define TEXT_THRESHOLD                 OMT_("Threshold")
#define TEXT_RATIO                     OMT_("Ratio")
#define TEXT_ATTACK                    OMT_("Attack")
#define TEXT_RELEASE                   OMT_("Release")
#define TEXT_M_GAIN                    OMT_("Makeup.Gain")
#define TEXT_KNEE                      OMT_("Knee")
#define TEXT_LINK                      OMT_("Link")
#define TEXT_DETECTION                 OMT_("Detection.Method")
#define TEXT_MIX                       OMT_("Wet.Dry.Mix")

struct ffmpeg_acompressor_data {
	obs_source_t                  *context;

	/* For the FFMPEG Setup: */
	struct fmpg_filter_settings    fmpg_set;

	/* acompressor plug-in settings. */
	float                          i_gain;
	double                         thresh;
	float                          ratio;
	float                          attack;
	float                          release;
	uint8_t                        m_gain;
	float                          knee;
	char                           link[8];
	char                           detec_meth[5];
	float                          mix;
};



#include <ao/ao.h>

static ao_device *device = NULL;



static int audio_decode_frame(AVFormatContext *ic, AVStream *audio_st,
	AVPacket *pkt, AVFrame *frame)
{
	AVPacket pkt_temp_;
	memset(&pkt_temp_, 0, sizeof(pkt_temp_));
	AVPacket *pkt_temp = &pkt_temp_;

	*pkt_temp = *pkt;

	int len1, got_frame;
	int new_packet = 1;
	while (pkt_temp->size > 0 || (!pkt_temp->data && new_packet)) {
		avcodec_get_frame_defaults(frame);
		new_packet = 0;

		len1 = avcodec_decode_audio4(audio_st->codec, frame, &got_frame, pkt_temp);
		if (len1 < 0) {
			// if error we skip the frame
			pkt_temp->size = 0;
			return -1;
		}

		pkt_temp->data += len1;
		pkt_temp->size -= len1;

		if (!got_frame) {
			// stop sending empty packets if the decoder is finished
			if (!pkt_temp->data &&
				audio_st->codec->codec->capabilities&CODEC_CAP_DELAY)
			{
				return 0;
			}
			continue;
		}

		// push the audio data from decoded frame into the filtergraph
		int err = av_buffersrc_write_frame(abuffer_ctx, frame);
		if (err < 0) {
			av_log(NULL, AV_LOG_ERROR, "error writing frame to buffersrc\n");
			return -1;
		}
		// pull filtered audio from the filtergraph
		for (;;) {
			int err = av_buffersink_get_frame(abuffersink_ctx, oframe);
			if (err == AVERROR_EOF || err == AVERROR(EAGAIN))
				break;
			if (err < 0) {
				av_log(NULL, AV_LOG_ERROR, "error reading buffer from buffersink\n");
				return -1;
			}
			int nb_channels = av_get_chan_layout_nb_channels(oframe->chan_layout);
			int bytes_per_sample = av_get_bytes_per_sample(oframe->format);
			int data_size = oframe->nb_samples * nb_channels * bytes_per_sample;
			ao_play(device, (void*)oframe->data[0], data_size);
		}
		return 0;
	}
	return 0;
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "Usage: %s file\n", argv[0]);
		return 1;
	}


	ao_initialize();
	avcodec_register_all();
	av_register_all();
	avformat_network_init();
	avfilter_register_all();

	ao_sample_format fmt;
	memset(&fmt, 0, sizeof(fmt));
	fmt.bits = 16;
	fmt.channels = 2;
	fmt.rate = 44100;
	fmt.byte_format = AO_FMT_NATIVE;
	device = ao_open_live(ao_default_driver_id(), &fmt, NULL);
	if (!device) {
		av_log(NULL, AV_LOG_ERROR, "opening audio device\n");
		return 1;
	}

	AVFormatContext *ic = NULL;
	char *filename = argv[1];
	if (avformat_open_input(&ic, filename, NULL, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "error opening %s\n", filename);
		return 1;
	}

	if (avformat_find_stream_info(ic, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "%s: could not find codec parameters\n", filename);
		return 1;
	}

	// set all streams to discard. in a few lines here we will find the audio
	// stream and cancel discarding it
	for (int i = 0; i < ic->nb_streams; i++)
		ic->streams[i]->discard = AVDISCARD_ALL;

	AVCodec *decoder = NULL;
	int audio_stream_index = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, -1, -1,
		&decoder, 0);

	if (audio_stream_index < 0) {
		av_log(NULL, AV_LOG_ERROR, "%s: no audio stream found\n", ic->filename);
		return 1;
	}

	if (!decoder) {
		av_log(NULL, AV_LOG_ERROR, "%s: no decoder found\n", ic->filename);
		return 1;
	}

	AVStream *audio_st = ic->streams[audio_stream_index];
	audio_st->discard = AVDISCARD_DEFAULT;

	AVCodecContext *avctx = audio_st->codec;

	if (avcodec_open2(avctx, decoder, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "unable to open decoder\n");
		return 1;
	}

	if (!avctx->chan_layout)
		avctx->chan_layout = av_get_default_chan_layout(avctx->channels);
	if (!avctx->chan_layout) {
		av_log(NULL, AV_LOG_ERROR, "unable to guess channel layout\n");
		return 1;
	}

	if (init_filter_graph(ic, audio_st) < 0) {
		av_log(NULL, AV_LOG_ERROR, "unable to init filter graph\n");
		return 1;
	}

	AVPacket audio_pkt;
	memset(&audio_pkt, 0, sizeof(audio_pkt));
	AVPacket *pkt = &audio_pkt;
	AVFrame *frame = avcodec_alloc_frame();

	oframe = av_frame_alloc();
	if (!oframe) {
		av_log(NULL, AV_LOG_ERROR, "error allocating oframe\n");
		return 1;
	}

	int eof = 0;
	for (;;) {
		if (eof) {
			if (avctx->codec->capabilities & CODEC_CAP_DELAY) {
				av_init_packet(pkt);
				pkt->data = NULL;
				pkt->size = 0;
				pkt->stream_index = audio_stream_index;
				if (audio_decode_frame(ic, audio_st, pkt, frame) > 0) {
					// keep flushing
					continue;
				}
			}
			break;
		}
		int err = av_read_frame(ic, pkt);
		if (err < 0) {
			if (err != AVERROR_EOF)
				av_log(NULL, AV_LOG_WARNING, "error reading frames\n");
			eof = 1;
			continue;
		}
		if (pkt->stream_index != audio_stream_index) {
			av_free_packet(pkt);
			continue;
		}
		audio_decode_frame(ic, audio_st, pkt, frame);
		av_free_packet(pkt);
	}

	avformat_network_deinit();
	ao_close(device);
	ao_shutdown();

	return 0;
}



/*
 * As the functions' namesake, this provides the internal name of your Filter,
 * which is then translated/referenced in the "data/locale" files.
 */
static const char *ffmpeg_acompressor_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return OMT_("FFMPEG.ACompressor");
}

/*
 * Since this is C we have to be careful when destroying/removing items from
 * OBS. Jim has added several useful functions to help keep memory leaks to
 * a minimum, and handle the destruction and construction of these filters.
 *
 * Since we're using FFMPEG in here, we especially need to clean up after
 * ourselves.
 */
static void ffmpeg_acompressor_destroy(void *data)
{
	struct ffmpeg_acompressor_data *pdata = data;

	/* First destroy the various AV contexts. */
	if (pdata->fmpg_set.filter_graph) {
		avfilter_graph_free(&pdata->fmpg_set.filter_graph);
	}

	if (pdata->fmpg_set.frame) {
		av_frame_free(&pdata->fmpg_set.frame);
	}

	/* Now destroy/free the OBS contexts. */
	bfree(pdata);
}

/*
 * This function is called (see bottom of this file for more details)
 * whenever the OBS filter interface changes. So when the user is messing
 * with a slider this function is called to update the internal settings
 * in OBS, and hence the settings being passed to the CPU/GPU.
 */
static void ffmpeg_acompressor_update(void *data, obs_data_t *settings)
{
	struct ffmpeg_acompressor_data *pdata = data;

	/*
	 * Allocate the ffmpeg_data struct and use it to pass data back
	 * and forth with the various FFMPEG+OBS functions.
	 */

	 /* Now time to update the filter! */
	 /* 1. Update the settings from OBS into pdata. */
	pdata->i_gain = (float)obs_data_get_double(settings, SETTING_I_GAIN);
	pdata->thresh = obs_data_get_double(settings, SETTING_THRESHOLD);
	pdata->ratio = (float)obs_data_get_double(settings, SETTING_RATIO);
	pdata->attack = (float)obs_data_get_double(settings, SETTING_ATTACK);
	pdata->release = (float)obs_data_get_double(settings, SETTING_RELEASE);
	pdata->m_gain = (uint8_t)obs_data_get_int(settings, SETTING_M_GAIN);
	pdata->knee = (float)obs_data_get_double(settings, SETTING_KNEE);

	memset(&pdata->link, NULL, sizeof(pdata->link));
	if (obs_data_get_int(settings, SETTING_LINK) == 0) {
		strncpy(pdata->link, "average", 7);
	}
	else {
		strncpy(pdata->link, "maximum", 7);
	}

	memset(pdata->detec_meth, NULL, sizeof(pdata->detec_meth));
	if (obs_data_get_int(settings, SETTING_DETECTION) == 0) {
		strncpy(pdata->detec_meth, "rms", 3);
	}
	else {
		strncpy(pdata->detec_meth, "peak", 4);
	}
	pdata->mix = obs_data_get_int(settings, SETTING_MIX) / 100.0f;



	/* 2. Update the settings from pdata into the local fmpg_set. */
	// I don't think I need any of the below.
	// Instead I need to hard code the assignment to the new "STRING"
	// and make it unique for each filter.

	// Use avfilter_insert_filter to re-insert theupdating settings for the acompressor?


	memcpy(pdata->fmpg_set.options_str,
			pdata->fmpg_set.options_str,
			OPTIONS_LENGTH * sizeof(uint8_t));


	/*
	 * 3a. The filter chain should always exist at this point.
	 * I need to figure out a way to check for said chain and wrap
	 * this statement in said if.
	 */
	fmpeg_filter_graph_update(pdata->fmpg_set);

	// Move to another more proper place: (Maybe create?)
	ffmpeg_filter_graph_create(pdata->fmpg_set);

	/* Copy to update settings? Initialize with a null string? */
	// create volume filter
	double vol = 0.40;
	snprintf(strbuf, sizeof(strbuf), "volume=%f", vol);
	fprintf(stderr, "volume: %s\n", strbuf);
	pdata->fmpg_set.err_code = avfilter_graph_create_filter(&volume_ctx, volume, NULL,
		strbuf, NULL, filter_graph);
	if (pdata->fmpg_set.err_code < 0) {
		av_log(NULL, AV_LOG_ERROR, "error initializing volume filter\n");
		return pdata->fmpg_set.err_code;
	}

	/*
	avfilter_free;
	avfilter_graph_create_filter;
	avfilter_graph_free;
	avfilter_graph_send_command;
	avfilter_insert_filter;
	avfilter_
	*/
}

/*
 * When you apply a filter OBS creates it, and adds it to the source. OBS also
 * starts rendering it immediately. This function allocates the needed arrays
 * for OBS to pass data back and forth, is applied to the source, and
 * calls the update function so that the filter starts rendering with the
 * correct settings from the first sample.
 */
static void *ffmpeg_acompressor_create(obs_data_t *settings,
		obs_source_t *filter)
{
	/*
	* Because of limitations of pre-c99 compilers (looking straight at
	* you, MS-who-sucks-balls-all-the-time), you can't create an
	* array that doesn't have a known size at compile time. The below
	* function calculates the size needed and allocates memory to
	* handle the source.
	*/
	struct ffmpeg_acompressor_data *pdata = bzalloc(sizeof(*pdata));

	/* basic setup for an Audio filter. */
	pdata->context = filter;

	/*
	* Need to get info about the source this filter is
	* attached to and OBS in general.
	*/
	struct audio_output_info const *obs_current_audio_settings =
		bzalloc(sizeof(struct audio_output_info));
	obs_current_audio_settings = audio_output_get_info(obs_get_audio());

	/* First we gotta call this to be able to grab ANY FFMPEG filter. */
	av_register_all();
	avfilter_register_all();

	/* Setting up the various calls pre-building the FFMPEG filters.*/
	pdata->fmpg_set.err_code = 0;

	memset(&pdata->fmpg_set.abuffer_str_opt[0], NULL, sizeof(pdata->fmpg_set.abuffer_str_opt));
	memset(&pdata->fmpg_set.filter_str_opt[0], NULL, sizeof(pdata->fmpg_set.filter_str_opt));
	memset(&pdata->fmpg_set.aformat_str_opt[0], NULL, sizeof(pdata->fmpg_set.aformat_str_opt));
	memset(&pdata->fmpg_set.filter_name[0], NULL, sizeof(pdata->fmpg_set.filter_name));
	memset(&pdata->fmpg_set.err_string[0], NULL, sizeof(pdata->fmpg_set.err_string));
	memset(&pdata->fmpg_set.fmpg_audio_format_str[0], NULL, sizeof(pdata->fmpg_set.fmpg_audio_format_str));
	memset(&pdata->fmpg_set.fmpg_speaker_layout_str[0], NULL, sizeof(pdata->fmpg_set.fmpg_audio_format_str));
	
	/* Filter's (FFMPEG) name...*/
	strcpy(pdata->fmpg_set.filter_name, "acompressor\0");

	/* Translate some stuff from OBS-speak to FFMPEG. */
	pdata->fmpg_set.obs_audio_format =
		obs_current_audio_settings->format;
	blog(LOG_INFO, "Printing pdata->fmpg_set.obs_audio_format value. The result is: %d",
		pdata->fmpg_set.obs_audio_format);

	pdata->fmpg_set.fmpg_audio_format =
		convert_audio_format(pdata->fmpg_set.obs_audio_format);
	blog(LOG_INFO, "Printing pdata->fmpg_set.fmpg_audio_format value. The result is: %d",
		pdata->fmpg_set.fmpg_audio_format);

	strncpy(pdata->fmpg_set.fmpg_audio_format_str,
		convert_audio_format_string(pdata->fmpg_set.obs_audio_format), sizeof(pdata->fmpg_set.fmpg_audio_format_str));
	blog(LOG_INFO, "pdata->fmpg_set.fmpg_audio_format_str value. The result is: %s",
		pdata->fmpg_set.fmpg_audio_format_str);
	blog(LOG_INFO, "Printing obs_current_audio_settings->speakers value. The result is: %d",
		obs_current_audio_settings->speakers);

	pdata->fmpg_set.fmpg_speaker_layout =
		convert_speaker_layout(obs_current_audio_settings->speakers);
	blog(LOG_INFO, "Printing pdata->fmpg_set.fmpg_speaker_layout value. The result is: %d",
		pdata->fmpg_set.fmpg_speaker_layout);

	strncpy(pdata->fmpg_set.fmpg_speaker_layout_str, convert_speaker_layout_string(obs_current_audio_settings->speakers), sizeof(pdata->fmpg_set.fmpg_speaker_layout_str));
	blog(LOG_INFO, "pdata->fmpg_set.fmpg_speaker_layout_str value. The result is: %s",
		pdata->fmpg_set.fmpg_speaker_layout_str);

	pdata->fmpg_set.obs_sample_rate =
		obs_current_audio_settings->samples_per_sec;
	blog(LOG_INFO, "Printing pdata->fmpg_set.obs_sample_rate value. The result is: %d",
		pdata->fmpg_set.obs_sample_rate);

	pdata->fmpg_set.obs_frame_size =
		audio_output_get_block_size(obs_get_audio());
	blog(LOG_INFO, "Printing pdata->fmpg_set.obs_frame_size value. The result is: %d",
		pdata->fmpg_set.obs_frame_size);

	/*
	 * Create a new filter graph, which will contain all
	 * the filters needed for this audio plug-in.
	 */
	pdata->fmpg_set.filter_graph = avfilter_graph_alloc();
	if (!pdata->fmpg_set.filter_graph) {
		blog(LOG_ERROR, "%s: Error code %d. Unable to create the filter graph.",
			pdata->fmpg_set.filter_name,
			AVERROR(ENOMEM));
		goto FAIL;
	}

	/*
	 * Create the abuffer filter:
	 * It's used for feeding OBS' audio data into the graph.
	 */
    /* Check for the abuffer filter. */
	pdata->fmpg_set.abuffer_filter =
			avfilter_get_by_name("abuffer");
	if (!pdata->fmpg_set.abuffer_filter) {
		blog(LOG_ERROR, "%s: Error code %d. Could not find the abuffer filter.", pdata->fmpg_set.filter_name,
			AVERROR_FILTER_NOT_FOUND);
		goto FAIL;
	}

    /* Now create settings for the abuffer filter. */
	snprintf(pdata->fmpg_set.abuffer_str_opt,
		sizeof(pdata->fmpg_set.abuffer_str_opt),
		"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
		1,
		pdata->fmpg_set.obs_sample_rate,
		pdata->fmpg_set.obs_sample_rate,
		av_get_sample_fmt_name(pdata->fmpg_set.fmpg_audio_format),
		pdata->fmpg_set.fmpg_speaker_layout);
	blog(LOG_INFO, "%s->abuffer string: %s",
		pdata->fmpg_set.filter_name,
		pdata->fmpg_set.abuffer_str_opt);

    /* Now create the abuffer filter. */
	pdata->fmpg_set.err_code = avfilter_graph_create_filter(
		&pdata->fmpg_set.abuffer_ctx,
		pdata->fmpg_set.abuffer_filter,
		NULL,
		pdata->fmpg_set.abuffer_str_opt,
		NULL,
		pdata->fmpg_set.filter_graph);

	if (pdata->fmpg_set.err_code < 0) {
		blog(LOG_ERROR, "%s: Error code %d, %d. Could not allocate the abuffer instance.", pdata->fmpg_set.filter_name,
			AVERROR(ENOMEM), pdata->fmpg_set.err_code);
		goto FAIL;
	}


	/*
	 * Create the aformat filter;
	 * it ensures that the output is of the format we want.
	 */
    /* Check for aformat filter. */
	pdata->fmpg_set.aformat_filter =
			avfilter_get_by_name("aformat");
	if (!pdata->fmpg_set.aformat_filter) {
		blog(LOG_ERROR, "%s: Error code %d. Could not find the aformat filter.",
			pdata->fmpg_set.filter_name,
			AVERROR_FILTER_NOT_FOUND);
		goto FAIL;
	}

    /* Now create settings to aformat filter. */
    snprintf(pdata->fmpg_set.aformat_str_opt,
	     sizeof(pdata->fmpg_set.aformat_str_opt),
	     "sample_fmts=%s:sample_rates=%d:channel_layouts=0x%"PRIx64,
	     av_get_sample_fmt_name(pdata->fmpg_set.fmpg_audio_format),
	     pdata->fmpg_set.obs_sample_rate,
	    pdata->fmpg_set.fmpg_speaker_layout);
    blog(LOG_INFO, "%s->aformat string: %s",
	 pdata->fmpg_set.filter_name,
	 pdata->fmpg_set.aformat_str_opt);

    /* Now create aformat filter. */
    pdata->fmpg_set.err_code =
	avfilter_graph_create_filter(
	    &pdata->fmpg_set.aformat_ctx,
	    pdata->fmpg_set.aformat_filter,
	    NULL,
	    pdata->fmpg_set.aformat_str_opt,
	    NULL,
	    pdata->fmpg_set.filter_graph);
    if (pdata->fmpg_set.err_code < 0) {
	blog(LOG_ERROR, "%s: Error code %d, %d. Could not allocate the aformat instance.",
	     pdata->fmpg_set.filter_name,
	     AVERROR(ENOMEM),
	     pdata->fmpg_set.err_code);
	goto FAIL;
    }

	/* Create acompressor filter. */
    /* Get latest settings from OBS. */
    pdata->i_gain = (float)obs_data_get_double(settings, SETTING_I_GAIN);
    pdata->thresh = obs_data_get_double(settings, SETTING_THRESHOLD);
    pdata->ratio = (float)obs_data_get_double(settings, SETTING_RATIO);
    pdata->attack = (float)obs_data_get_double(settings, SETTING_ATTACK);
    pdata->release = (float)obs_data_get_double(settings, SETTING_RELEASE);
    pdata->m_gain = (uint8_t)obs_data_get_int(settings, SETTING_M_GAIN);
    pdata->knee = (float)obs_data_get_double(settings, SETTING_KNEE);
    memset(pdata->link, NULL, sizeof(pdata->link));
    if (obs_data_get_int(settings, SETTING_LINK) == 0) {
	    strncpy(pdata->link, "average", 7);
    }
    else {
	    strncpy(pdata->link, "maximum", 7);
    }
    blog(LOG_INFO, "%s: pdata->link value is: %s, %d",
	    pdata->fmpg_set.filter_name,
	    pdata->link,
	    obs_data_get_int(settings, SETTING_LINK));
    pdata->detec_meth = obs_data_get_string(settings, SETTING_DETECTION);
    memset(pdata->detec_meth, NULL, sizeof(pdata->detec_meth));
    if (obs_data_get_int(settings, SETTING_DETECTION) == 0) {
	    strncpy(pdata->detec_meth, "rms", 3);
    }
    else {
	    strncpy(pdata->detec_meth, "peak", 4);
    }
    blog(LOG_INFO, "%s: pdata->detec_meth value is: %s, %d",
	    pdata->fmpg_set.filter_name,
	    pdata->detec_meth,
	    obs_data_get_int(settings, SETTING_DETECTION));

    pdata->mix = obs_data_get_int(settings, SETTING_MIX) / 100.0f;

     /* Check for acompressor filter. */
    pdata->fmpg_set.audio_filter = avfilter_get_by_name(pdata->fmpg_set.filter_name);
    if (!pdata->fmpg_set.audio_filter) {
	blog(LOG_ERROR, "%s: Error code %d, %d. Can not find filter.",
	     pdata->fmpg_set.filter_name,
	     AVERROR(ENOMEM),
	     pdata->fmpg_set.err_code);
	goto FAIL;
    }
    /* Create Context for acompressor filter. */
    pdata->fmpg_set.audio_filter_ctx =
	avfilter_graph_alloc_filter(pdata->fmpg_set.filter_graph,
		pdata->fmpg_set.audio_filter,
		pdata->fmpg_set.filter_name);
    if (!pdata->fmpg_set.audio_filter_ctx) {
	blog(LOG_ERROR, "%s: Error code %d, %d. Could not allocate the filter instance or context.",
	     pdata->fmpg_set.filter_name,
	     AVERROR(ENOMEM),
	     pdata->fmpg_set.err_code);
	goto FAIL;
    }

    /* Translate settings into string. */
	snprintf(pdata->fmpg_set.filter_str_opt,
	     sizeof(pdata->fmpg_set.filter_str_opt),
		"level_in=%f:threshold=%lf:ratio=%f:attack=%f:release=%f:makeup=%i:knee=%f:link=%s:detection=%s:mix=%f",
	     pdata->i_gain,
	     pdata->thresh,
	     pdata->ratio,
	     pdata->attack,
	     pdata->release,
	     pdata->m_gain,
	     pdata->knee,
	     pdata->link,
	     pdata->detec_meth,
	     pdata->mix);

	blog(LOG_INFO, "%s settings: %s",
	 pdata->fmpg_set.filter_name,
	 pdata->fmpg_set.filter_str_opt);

	pdata->fmpg_set.err_code = avfilter_graph_create_filter(
	&pdata->fmpg_set.audio_filter_ctx,
	pdata->fmpg_set.audio_filter,
	NULL,
		pdata->fmpg_set.filter_str_opt,
	NULL,
	pdata->fmpg_set.filter_graph);
	if (pdata->fmpg_set.err_code < 0) {
	blog(LOG_ERROR, "%s: Error code %d, %d. Error initializing filter.",
	     pdata->fmpg_set.filter_name,
	     AVERROR(ENOMEM),
	     pdata->fmpg_set.err_code);
	goto FAIL;
	}

    /* Now create the audio filter proper. */
    pdata->fmpg_set.audio_filter = avfilter_get_by_name(pdata->fmpg_set.filter_name);
    if (!pdata->fmpg_set.audio_filter) {
	blog(LOG_ERROR, "%s: Error code %d, %d. Could not create the %s filter.",
	     pdata->fmpg_set.filter_name,
	     AVERROR_FILTER_NOT_FOUND,
	     pdata->fmpg_set.err_code,
	     pdata->fmpg_set.filter_name);
	goto FAIL;
    }

    /*
    * Create the abuffersink filter;
    * It ensures that the output is buffered and we don't lose any data
    * on the trip back into OBS' audio chain.
    */
    /* Now create the abuffersink filter. */
    pdata->fmpg_set.abuffersink = avfilter_get_by_name("abuffersink");
    if (!pdata->fmpg_set.abuffersink) {
	    blog(LOG_ERROR, "%s: Could not find the abuffersink filter.",
		    pdata->fmpg_set.filter_name);
	goto FAIL;
    }

    pdata->fmpg_set.abuffersink_ctx = avfilter_graph_alloc_filter(pdata->fmpg_set.filter_graph, pdata->fmpg_set.abuffersink, "sink");
    if (!pdata->fmpg_set.abuffersink_ctx) {
	    blog(LOG_ERROR, "%s: Could not allocate the abuffersink filter.",
		    pdata->fmpg_set.filter_name);
	    goto FAIL;
    }

    pdata->fmpg_set.err_code = avfilter_init_str(pdata->fmpg_set.abuffersink_ctx, NULL);
    if (!pdata->fmpg_set.err_code < 0) {
	    blog(LOG_ERROR, "%s: Could not initialize the abuffersink instance.",
		    pdata->fmpg_set.filter_name);
	    goto FAIL;
    }

	/*
	 * This is a simple filter, so we connect the following:
	 * abuffer_ctx to (ffmpeg)audio_filter_ctx
	 * (ffmpeg)audio_filter_ctx to aformat_ctx
	 * aformat_ctx to abuffersink_ctx
	 */
	pdata->fmpg_set.err_code = avfilter_link(
	    pdata->fmpg_set.abuffer_ctx,
	    0,
	    pdata->fmpg_set.audio_filter_ctx,
	    0);
    if (pdata->fmpg_set.err_code < 0){
	    blog(LOG_ERROR, "%s: Error code %d, %d. Could not link the abuffer and audio_filter contexts.",
		    pdata->fmpg_set.filter_name,
		    AVERROR(AV_LOG_ERROR),
		    pdata->fmpg_set.err_code);
	    goto FAIL;
    }
    else {
	    pdata->fmpg_set.err_code = avfilter_link(
		    pdata->fmpg_set.audio_filter_ctx,
		    0,
		    pdata->fmpg_set.aformat_ctx,
		    0);
    }
    if (pdata->fmpg_set.err_code < 0){
	    blog(LOG_ERROR, "%s: Error code %d, %d. Could not link the audio_filter and aformat_filter contexts.",
		    pdata->fmpg_set.filter_name,
		    AVERROR(AV_LOG_ERROR),
		    pdata->fmpg_set.err_code);
	    goto FAIL;
    }
    else {
	    pdata->fmpg_set.err_code = avfilter_link(
		    pdata->fmpg_set.aformat_ctx,
		    0,
		    pdata->fmpg_set.abuffersink_ctx,
		    0);
    }

	if (pdata->fmpg_set.err_code < 0) {
		blog(LOG_ERROR, "%s: Error code %d, %d. Error connecting aformat to abuffersink contexts.",
	     pdata->fmpg_set.filter_name,
	     AVERROR(AV_LOG_ERROR),
	     pdata->fmpg_set.err_code);
	goto FAIL;
	}

	/* Create the in<->out filters. */
	pdata->fmpg_set.out_filter = avfilter_inout_alloc();
	pdata->fmpg_set.in_filter = avfilter_inout_alloc();
	/*
	* The buffer source output must be connected to the input pad of
	* the first filter described by filters_descr; since the first
	* filter input label is not specified, it is set to "in" by
	* default.
	*/
	pdata->fmpg_set.out_filter->name = av_strdup("in");
	pdata->fmpg_set.out_filter->filter_ctx =
		pdata->fmpg_set.abuffer_ctx;
	pdata->fmpg_set.out_filter->pad_idx = 0;
	pdata->fmpg_set.out_filter->next = NULL;
	/*
	* The buffer sink input must be connected to the output pad of
	* the last filter described by filters_descr; since the last
	* filter output label is not specified, it is set to "out" by
	* default.
	*/
	pdata->fmpg_set.in_filter->name = av_strdup("out");
	pdata->fmpg_set.in_filter->filter_ctx =
		pdata->fmpg_set.abuffersink_ctx;
	pdata->fmpg_set.in_filter->pad_idx = 0;
	pdata->fmpg_set.in_filter->next = NULL;
	/* ??? */
	if ((pdata->fmpg_set.err_code = avfilter_graph_parse_ptr(
		pdata->fmpg_set.filter_graph,
		pdata->fmpg_set.aformat_str_opt,
		&pdata->fmpg_set.in_filter,
		&pdata->fmpg_set.out_filter,
		NULL)) < 0)
	{
		blog(LOG_ERROR, "%s: Error code %d. Error parsing the filter graph.",
			pdata->fmpg_set.filter_name,
			AVERROR(ENOMEM));
		goto FAIL;
	}
		
	if ((pdata->fmpg_set.err_code = avfilter_graph_config(
		pdata->fmpg_set.filter_graph, NULL)) < 0)
	{
		blog(LOG_ERROR, "%s: Error code %d. Error pulling in the filter_graph_config.",
			pdata->fmpg_set.filter_name,
			AVERROR(ENOMEM));
		goto FAIL;
	}
		
	/* Lastly (yes, finally!) we build and configure the
	 * avfilter_graph itself. */
	pdata->fmpg_set.filter_graph = avfilter_graph_alloc();

	if (!pdata->fmpg_set.filter_graph) {
		blog(LOG_ERROR, "%s: Error code %d. Error allocating the filter graph.",
			pdata->fmpg_set.filter_name,
			AVERROR(ENOMEM));
		goto FAIL;
	}

	pdata->fmpg_set.err_code = avfilter_graph_create_filter(&pdata->fmpg_set.abuffer_ctx,
		pdata->fmpg_set.abuffer_filter,
		"in",
		pdata->fmpg_set.abuffer_str_opt,
		NULL,
		pdata->fmpg_set.filter_graph);
	if (pdata->fmpg_set.err_code < 0) {
		blog(LOG_ERROR, "%s: Error code %d. Error attaching the abuffer to the filter graph.",
			pdata->fmpg_set.filter_name,
			AVERROR(ENOMEM));
		goto FAIL;
	}


	/* buffer audio sink: to terminate the filter chain. */
	pdata->fmpg_set.err_code = avfilter_graph_create_filter(&pdata->fmpg_set.abuffersink_ctx,
		pdata->fmpg_set.abuffersink,
		"out", NULL, NULL,
		pdata->fmpg_set.filter_graph);
	if (pdata->fmpg_set.err_code < 0) {
		blog(LOG_ERROR, "%s: Error code %d. Error attaching the abuffer_sink to the filter graph.",
			pdata->fmpg_set.filter_name,
			AVERROR(ENOMEM));
		goto FAIL;
	}

	pdata->fmpg_set.err_code = avfilter_graph_config(
	    pdata->fmpg_set.filter_graph, NULL);

	if (pdata->fmpg_set.err_code < 0) {
		av_strerror(pdata->fmpg_set.err_code,
			pdata->fmpg_set.err_string, sizeof(pdata->fmpg_set.err_string));

	blog(LOG_ERROR, "%s: Error code %d, %d (%s). Error configuring the filter graph. %s",
	     pdata->fmpg_set.filter_name,
	     AVERROR(AV_LOG_ERROR),
	     pdata->fmpg_set.err_code,
		pdata->fmpg_set.err_string,
		pdata->fmpg_set.filter_graph);
	goto FAIL;
	}

	/* Free what is no longer needed. */
	avfilter_inout_free(&pdata->fmpg_set.in_filter);
	avfilter_inout_free(&pdata->fmpg_set.out_filter);

	/* Now update the filter with the latest settings. */
	ffmpeg_acompressor_update(pdata, settings);
	goto FAIL;

	FAIL:
	UNUSED_PARAMETER(pdata);
	return pdata;
}

/* This is where the actual rendering of the filter takes place. There's a
 * certain amount of setup and bookkeeping involved because we (the coders)
 * can't know ahead of time what the sample rate, number of channels, channel
 * configuration, number of samples/frames being handed to our plug-in (etc.)
 * is, so we need to carefully check everything going
 * in and out of our filter.
 */
static struct obs_audio_data *ffmpeg_acompressor_filter_audio(void *data,
	struct obs_audio_data *audio)
{
	struct ffmpeg_acompressor_data *pdata = data;

	float *adata[2] = { (float*)audio->data[0], (float*)audio->data[1] };

	for (size_t i = 0; i < audio->frames; i++) {

		/* See https://www.ffmpeg.org/doxygen/3.2/structAVFrame.html#afca04d808393822625e09b5ba91c6756
		 * for more details.
		 */
		 float *tdata = (float*)pdata->fmpg_set.frame->extended_data[i];

		 /* Send the frame to the input of the filtergraph. */
		pdata->fmpg_set.err_code =
			av_buffersrc_add_frame(
				pdata->fmpg_set.audio_filter_ctx,
				pdata->fmpg_set.frame);
		if (pdata->fmpg_set.err_code < 0) {
			av_frame_unref(pdata->fmpg_set.frame);
			blog(LOG_ERROR, "%s: Error submitting the frame to the filtergraph:",
				pdata->fmpg_set.filter_name);

			goto FAIL;
		}

		for (size_t c = 0;
			c < pdata->fmpg_set.obs_chan_count;
			c++) {
			/* Pass audio data from OBS to FFMPEG. */
			/*
			* Since we now know how many frames we need to process,
			* we multiply the number of frames given to us by OBS,
			* times the number of channels we have to process,
			* Times the size of the macro AUDIO_OUTPUT_FRAMES.
			*/
			pdata->fmpg_set.frame->pts =
				audio->frames *
				pdata->fmpg_set.obs_chan_count *
				AUDIO_OUTPUT_FRAMES;
			blog(LOG_INFO, "%s: PTS value %d.\n",
				pdata->fmpg_set.filter_name,
				pdata->fmpg_set.frame->pts);

			pdata->fmpg_set.err_code =
				av_frame_get_buffer(pdata->fmpg_set.frame, 0);

			if (pdata->fmpg_set.err_code < 0) {
				blog(LOG_ERROR, "%s: Error code %d. Error in av_frame_get_buffer in the ffmpeg_acompressor_filter_audio function(failure to allocate frame).\n",
					pdata->fmpg_set.filter_name,
					pdata->fmpg_set.err_code);
				goto FAIL;
			}

		}
	}

	/* Now get the data back from FFMPEG and put into OBS. */

	FAIL:
	return audio;
}

/*
 * As the functions' namesake, this provides the default settings for any
 * options you wish to provide a default for. Try to select defaults that
 * make sense to the end user, or that don't effect the data.
 * *NOTE* this function is completely optional, as is providing a default
 * for any particular setting.
 */
static void ffmpeg_acompressor_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, SETTING_I_GAIN, 32.0 /*1.0*/);
	obs_data_set_default_double(settings, SETTING_THRESHOLD, 0.125);
	obs_data_set_default_double(settings, SETTING_RATIO, 2.0f);

	/* Attack is in milliseconds. */
	obs_data_set_default_double(settings, SETTING_ATTACK, 20);

	/* Release is in milliseconds. */
	obs_data_set_default_double(settings, SETTING_RELEASE, 250);
	obs_data_set_default_int(settings, SETTING_M_GAIN, 2);
	obs_data_set_default_double(settings, SETTING_KNEE, 2.82843);

	/* Here TRUE=average, FALSE=Maximum. */
	obs_data_set_default_int(settings, SETTING_LINK, 0);

	/* Here TRUE=rms, FALSE=peak. */
	obs_data_set_default_int(settings, SETTING_DETECTION, 0);
	obs_data_set_default_int(settings, SETTING_MIX, 100);
}

/*
 * This function sets the interface, the types (add_*_Slider), the type of
 * data collected (int), the internal name, user-facing name, minimum,
 * maximum and step values. While a custom interface can be built, for a
 * simple filter like this it's better to use the supplied functions.
 */
static obs_properties_t *ffmpeg_acompressor_properties(void *data)
{
	obs_properties_t *ppts = obs_properties_create();

	obs_properties_add_float_slider(ppts, SETTING_I_GAIN, TEXT_I_GAIN,
		0.015625f, 64.0f, 0.001f);
	obs_properties_add_float(ppts, SETTING_THRESHOLD, TEXT_THRESHOLD,
		0.00097563f, 1.0f, 0.00000001f);
	obs_properties_add_float_slider(ppts, SETTING_RATIO, TEXT_RATIO,
		1.0f, 20.0f, 0.1f);
	obs_properties_add_float(ppts, SETTING_ATTACK, TEXT_ATTACK,
		0.01f, 2000, 0.01f);
	obs_properties_add_float_slider(ppts, SETTING_RELEASE, TEXT_RELEASE,
		0.01f, 9000, 0.01f);
	obs_properties_add_int_slider(ppts, SETTING_M_GAIN, TEXT_M_GAIN,
		1, 64, 1);
	obs_properties_add_float(ppts, SETTING_KNEE, TEXT_KNEE,
		1.0f, 8.0f, 0.0001f);

	/*obs_property_t *link = obs_properties_add_list(ppts, SETTING_LINK, TEXT_LINK,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(link,
		OMT_("Average"), "average\0");
	obs_property_list_add_string(link,
		OMT_("Maximum"), "maximum\0");*/

	obs_property_t *link = obs_properties_add_list(ppts, SETTING_LINK, TEXT_LINK,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(link,
		OMT_("Average"), 0);
	obs_property_list_add_int(link,
		OMT_("Maximum"), 1);

	/*obs_property_t *dect = obs_properties_add_list(ppts,
		SETTING_DETECTION, TEXT_DETECTION,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(dect,
		OMT_("RMS"), "rms\0");
	obs_property_list_add_string(dect,
		OMT_("Peak"), "peak\0");*/

	obs_property_t *dect = obs_properties_add_list(ppts,
		SETTING_DETECTION, TEXT_DETECTION,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(dect,
		OMT_("RMS"), 0);
	obs_property_list_add_int(dect,
		OMT_("Peak"), 1);

	obs_properties_add_int(ppts, SETTING_MIX, TEXT_MIX,
		0, 100, 1);

	UNUSED_PARAMETER(data);
	return ppts;
}

/*
 * So how does OBS keep track of all these plug-ins/filters? How does OBS know
 * which function to call when it needs to update a setting? Or a source? Or
 * what type of source this is?
 *
 * OBS does it through the obs_source_info_struct. Notice how variables are
 * assigned the name of a function? Notice how the function name has the
 * variable name in it? While not mandatory, it helps a ton for you (and those
 * reading your code) to follow this convention.
 */
struct obs_source_info ffmpeg_acompressor_filter = {
	.id = "ffmpeg_acompressor_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = ffmpeg_acompressor_name,
	.create = ffmpeg_acompressor_create,
	.destroy = ffmpeg_acompressor_destroy,
	.update = ffmpeg_acompressor_update,
	.filter_audio = ffmpeg_acompressor_filter_audio,
	.get_defaults = ffmpeg_acompressor_defaults,
	.get_properties = ffmpeg_acompressor_properties,
};
