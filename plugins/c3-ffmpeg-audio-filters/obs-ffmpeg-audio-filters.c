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

#include <obs-module.h>


/*
 * Here we enable OBS to grab our filters and instantiate our filters.
 */
 extern struct obs_source_info ffmpeg_acompressor_filter;
 /*extern struct obs_source_info ffmpeg_acrusher_filter;
extern struct obs_source_info ffmpeg_adelay_filter;
extern struct obs_source_info ffmpeg_aecho_filter;
extern struct obs_source_info ffmpeg_aemphasis_filter;
extern struct obs_source_info ffmpeg_afftfilt_filter;
extern struct obs_source_info ffmpeg_agate_filter;
extern struct obs_source_info ffmpeg_alimiter_filter;
extern struct obs_source_info ffmpeg_allpass_filter;
extern struct obs_source_info ffmpeg_anequalizer_filter;
extern struct obs_source_info ffmpeg_aphaser_filter;
extern struct obs_source_info ffmpeg_apulsator_filter;
extern struct obs_source_info ffmpeg_bandpass_filter;
extern struct obs_source_info ffmpeg_bandreject_filter;
extern struct obs_source_info ffmpeg_bass_filter;
extern struct obs_source_info ffmpeg_chorus_filter;
extern struct obs_source_info ffmpeg_compand_filter;
extern struct obs_source_info ffmpeg_crystalizer_filter;
extern struct obs_source_info ffmpeg_dcshift_filter;
extern struct obs_source_info ffmpeg_dynaudnorm_filter;
extern struct obs_source_info ffmpeg_equalizer_filter;
extern struct obs_source_info ffmpeg_extra_stereo_filter;
extern struct obs_source_info ffmpeg_fir_equalizer_filter;
extern struct obs_source_info ffmpeg_flanger_filter;
extern struct obs_source_info ffmpeg_highpass_filter;
extern struct obs_source_info ffmpeg_loudnorm_filter;
extern struct obs_source_info ffmpeg_lowpass_filter;
extern struct obs_source_info ffmpeg_rubberband_filter;
extern struct obs_source_info ffmpeg_side_chain_compress_filter;
extern struct obs_source_info ffmpeg_side_chain_gate_filter;
extern struct obs_source_info ffmpeg_stereo_tools_filter;
extern struct obs_source_info ffmpeg_stereo_widen_filter;
extern struct obs_source_info ffmpeg_treble_filter;
extern struct obs_source_info ffmpeg_tremolo_filter;
extern struct obs_source_info ffmpeg_vibrato_filter;
*/
 /*
#if BUILD_TESTS
	extern struct obs_source_info ffmpeg_acopy_filter;
	extern struct obs_source_info ffmpeg_aeval_filter;
	extern struct obs_source_info ffmpeg_biquad_filter;
#endif
*/


/*
 * Here we have OBS create a handle to our plug-in so that users can access
 * and apply it to a source.
 */
bool inline obs_module_load(void)
{
	obs_register_source(&ffmpeg_acompressor_filter);
	/*obs_register_source(&ffmpeg_acrusher_filter);
	obs_register_source(&ffmpeg_adelay_filter);
	obs_register_source(&ffmpeg_aecho_filter);
	obs_register_source(&ffmpeg_aemphasis_filter);
	obs_register_source(&ffmpeg_afftfilt_filter);
	obs_register_source(&ffmpeg_agate_filter);
	obs_register_source(&ffmpeg_alimiter_filter);
	obs_register_source(&ffmpeg_allpass_filter);
	obs_register_source(&ffmpeg_anequalizer_filter);
	obs_register_source(&ffmpeg_aphaser_filter);
	obs_register_source(&ffmpeg_apulsator_filter);
	obs_register_source(&ffmpeg_bandpass_filter);
	obs_register_source(&ffmpeg_bass_filter);
	obs_register_source(&ffmpeg_chorus_filter);
	obs_register_source(&ffmpeg_compand_filter);
	obs_register_source(&ffmpeg_crystalizer_filter);
	obs_register_source(&ffmpeg_dcshift_filter);
	obs_register_source(&ffmpeg_dynaudnorm_filter);
	obs_register_source(&ffmpeg_equalizer_filter);
	obs_register_source(&ffmpeg_extra_stereo_filter);
	obs_register_source(&ffmpeg_fir_equalizer_filter);
	obs_register_source(&ffmpeg_flanger_filter);
	obs_register_source(&ffmpeg_highpass_filter);
	obs_register_source(&ffmpeg_loudnorm_filter);
	obs_register_source(&ffmpeg_lowpass_filter);
	obs_register_source(&ffmpeg_rubberband_filter);
	obs_register_source(&ffmpeg_side_chain_compress_filter);
	obs_register_source(&ffmpeg_side_chain_gate_filter);
	obs_register_source(&ffmpeg_stereo_tools_filter);
	obs_register_source(&ffmpeg_stereo_widen_filter);
	obs_register_source(&ffmpeg_treble_filter);
	obs_register_source(&ffmpeg_tremolo_filter);
	obs_register_source(&ffmpeg_vibrato_filter);

	#if BUILD_TESTS
		obs_register_source (&ffmpeg_acopy_filter);
		obs_register_source (&ffmpeg_aeval_filter);
		obs_register_source (&ffmpeg_biquad_filter);
	#endif
	*/

	return true;
}

/* Now time to create the FFMPEG audio graph. */
/*
extern void ffmpeg_filter_graph_create(void *ffmpeg_settings)
{
	struct ffmpeg_filter_settings *ffms = ffmpeg_settings;


	// Translate some stuff from OBS-speak to FFMPEG.
	blog(LOG_INFO, "trying enum ffms->obs_audio_format. the result is: %d \n",
		ffms->obs_audio_format);
	enum AVSampleFormat ffms_audio_format =
		convert_audio_format(ffms->obs_audio_format);
	blog(LOG_INFO, "trying enum ffms_audio_format. the result is: %d \n",
		ffms_audio_format);


	blog(LOG_INFO, "trying enum ffms->obs_speaker_layout. the result is: %d \n",
		ffms->obs_speaker_layout);
	enum AVSpeakerFormat ffms_channel_layout =
		convert_speaker_layout(ffms->obs_speaker_layout);
	blog(LOG_INFO, "trying enum ffms_channel_layout. the result is: %d \n",
		ffms_channel_layout);


	char *ffms_audio_format_str[22] =
	{ convert_audio_format_string(ffms->obs_audio_format) };
	blog(LOG_INFO, "converted from emun to string for ffms_audio_format_str. the result is: %s \n",
		ffms_audio_format_str);

	char *ffms_channel_layout_str[35] =
	{ convert_speaker_layout_string(ffms->obs_speaker_layout) };
	blog(LOG_INFO, "converted from emun to string for ffms_channel_layout_str. the result is: %s \n",
		ffms_channel_layout_str);
	strcpy(ffms_channel_layout_str, "AV_CH_LAYOUT_STEREO");
	blog(LOG_INFO, "hardcoded string for ffms_channel_layout_str. the result is: %s \n",
		ffms_channel_layout_str);

	//
	// Create a new filtergraph, which will contain all
	// the filters needed for this audio plugin.
	//
	ffms->filter_graph = avfilter_graph_alloc();
	if (!ffms->filter_graph) {
		blog(LOG_INFO, "%s: Error code %d. Unable to create the filter graph.\n",
			ffms->plugin_name,
			AVERROR(ENOMEM));
		goto fail;
	}

	//*
	//* Create the abuffer filter:
	//* It's used for feeding OBS' audio data into the graph.
	//
	ffms->abuffer = avfilter_get_by_name("abuffer");
	if (!ffms->abuffer) {
		blog(LOG_INFO, "%s: Error code %d. Could not find the abuffer filter.\n",
			ffms->plugin_name,
			AVERROR_FILTER_NOT_FOUND);
		goto fail;
	}

	//*
	//* Now we attach the abuffer filter to the filtergraph via
	//* a context, called (surprisingly enough) abuffer_context.
	//
	ffms->abuffer_context = avfilter_graph_alloc_filter(ffms->filter_graph,
		ffms->abuffer, "src");
	if (!ffms->abuffer_context) {
		blog(LOG_INFO, "%s: Error code %d. Could not allocate the abuffer instance.\n",
			ffms->plugin_name,
			AVERROR(ENOMEM));
		goto fail;
	}

	//* Set the abuffer filter options through the AVOptions API.
	blog(LOG_INFO, "ffms->obs_sample_rate. The result is: %d \n",
		ffms->obs_sample_rate);

	av_get_channel_layout_string(ffms_channel_layout_str,
		sizeof(ffms_channel_layout_str), 0, ffms_audio_format);
	av_opt_set(ffms->abuffer_context, "channel_layout",
		ffms_channel_layout_str, AV_OPT_SEARCH_CHILDREN);
	av_opt_set(ffms->abuffer_context, "sample_fmt", av_get_sample_fmt_name
	(ffms_audio_format), AV_OPT_SEARCH_CHILDREN);
	av_opt_set_q(ffms->abuffer_context, "time_base", (AVRational) {
		1,
			ffms->obs_sample_rate
	}, AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(ffms->abuffer_context, "sample_rate",
		ffms->obs_sample_rate, AV_OPT_SEARCH_CHILDREN);

	//*
	//* Now initialize the filter; we pass NULL options, since we have
	//* already set all the options above.
	//
	ffms->error_code = avfilter_init_str(ffms->abuffer_context, NULL);
	if (ffms->error_code < 0) {
		blog(LOG_INFO, "%s: Error code %d. Could not initialize the abuffer filter.\n",
			ffms->plugin_name,
			ffms->error_code);
		goto fail;
	}


	//* Now create and attach the audio filter.



	//*
	ffms->error_code = avfilter_init_str(ffms->aformat_context,
		ffms->options_str);
	if (ffms->error_code < 0) {
		blog(LOG_INFO, "%s: Error code %d. Could not allocate this %s filter instance.\n",
			ffms->plugin_name,
			ffms->error_code,
			ffms->plugin_name);
		goto fail;
	}

	//*
	//* Create the aformat filter;
	//* it ensures that the output is of the format we want.
	//
	ffms->aformat = avfilter_get_by_name("aformat");
	if (!ffms->aformat) {
		blog(LOG_INFO, "%s: Error code %d. Could not find the aformat filter.\n",
			ffms->plugin_name,
			AVERROR_FILTER_NOT_FOUND);
		goto fail;
	}

	ffms->aformat_context = avfilter_graph_alloc_filter(ffms->filter_graph,
		ffms->aformat, "aformat");
	if (!ffms->aformat_context) {
		blog(LOG_INFO, "%s: Error code %d. Could not allocate the aformat instance.\n",
			ffms->plugin_name,
			AVERROR(ENOMEM));
		goto fail;
	}

	if (ffms->error_code < 0) {
		blog(LOG_INFO, "%s: Error code %d. Could not initialize the aformat filter.\n",
			ffms->plugin_name,
			ffms->error_code);
		goto fail;
	}


	//*
	//* In between these 2 function calls one should construct the
	//* FFMPEG audio filter and pass those values into the
	//* ffmpeg_filter_data struct so the filter can be properly
	//* constructed.
	//



	//*
	//* Now create the abuffersink filter;
	//* it will be used to get the filtered data out of the graph.
	//
	ffms->abuffersink = avfilter_get_by_name("abuffersink");

	if (!ffms->abuffersink) {
		blog(LOG_INFO, "%s: Error code %d. Could not find the abuffersink filter.\n",
			ffms->plugin_name,
			AVERROR_FILTER_NOT_FOUND);
		goto fail;
	}

	ffms->abuffersink_context = avfilter_graph_alloc_filter(
		ffms->filter_graph, ffms->abuffersink, "sink");

	if (!ffms->abuffersink_context) {
		blog(LOG_INFO, "%s: Error code %d. Could not allocate the abuffersink instance.\n",
			ffms->plugin_name,
			AVERROR(ENOMEM));
		goto fail;
	}

	//*
	//* This filter takes no options, and is the output sink for
	//* our audio processing.
	//
	ffms->error_code = avfilter_init_str(ffms->abuffersink_context, NULL);

	if (ffms->error_code < 0) {
		blog(LOG_INFO, "%s: Error code %d. Could not initialize the abuffersink instance.\n",
			ffms->plugin_name,
			ffms->error_code);
		goto fail;
	}

	//*
	//* Connect the filters;
	//* in this simple case the filters just form a linear chain.
	//
	ffms->error_code = avfilter_link(ffms->abuffer_context, 0,
		ffms->audio_filter_context, 0);

	if (ffms->error_code >= 0) {
		ffms->error_code = avfilter_link(ffms->audio_filter_context, 0,
			ffms->aformat_context, 0);
	}

	if (ffms->error_code >= 0) {
		ffms->error_code = avfilter_link(ffms->aformat_context, 0,
			ffms->abuffersink_context, 0);
	}

	if (ffms->error_code < 0) {
		blog(LOG_INFO, "%s: Error code %d. Could not connect the filters.\n",
			ffms->plugin_name,
			ffms->error_code);
		goto fail;
	}

	//* Configure the graph.
	ffms->error_code = avfilter_graph_config(ffms->filter_graph, NULL);

	if (ffms->error_code < 0) {
		blog(LOG_INFO, "%s: Error code %d. Error configuring the filter graph.\n",
			ffms->plugin_name,
			ffms->error_code);
		goto fail;
	}

	//*
	//* We need to pass back that the creation of the filter was
	//* successful so when we try to change settings, it isn't
	//* created a 2nd time.
	//
	ffms->current_filter_exists = TRUE;

	//*
	//ffd->graph = ffd->filter_graph;
	//ffd->src   = ffd->abuffer_context;
	//ffd->sink  = ffd->abuffersink_context;
	//

fail:
	UNUSED_PARAMETER(ffms);
}
*/
