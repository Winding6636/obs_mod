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

Audio processing code Copyright (c) Markus Schmidt and Christian Holschuh
Adapted from the FFMPEG project, which is licensed below:
* FFmpeg is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* FFmpeg is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
******************************************************************************/

#include <obs-module.h>
#include "headers/obs-ffmpeg-audio-filters.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-ffmpeg-audio-filters", "en-US")


#define SET_LEVEL_IN                  "level_in"
#define SET_LEVEL_OUT                 "level_out"
#define SET_BIT_REDUCTION             "bit_reduction"
#define SET_WET_DRY_MIX               "wet_dry_mix"
#define SET_MODE                      "mode"
#define SET_DC_OFFSET                 "dc_offset"
#define SET_AA                        "anti_aliasing"
#define SET_SAMPLE_REDUCTION          "sample_reduction"
#define SET_ENABLE_LFO                "enable_lfo"
#define SET_LFO_RANGE                 "lfo_range"
#define SET_LFO_RATE                  "lfo_rate"


#define TEXT_LEVEL_IN                  OMT_("Level.In")
#define TEXT_LEVEL_OUT                 OMT_("Level.Out")
#define TEXT_BIT_REDUCTION             OMT_("Bit.Reduction")
#define TEXT_WET_DRY_MIX               OMT_("Wet.Dry.Mix")
#define TEXT_MODE                      OMT_("Mode")
#define TEXT_DC_OFFSET                 OMT_("DC.Offset")
#define TEXT_AA                        OMT_("Anti.Aliasing")
#define TEXT_SAMPLE_REDUCTION          OMT_("Sample.Reduction")
#define TEXT_ENABLE_LFO                OMT_("Enable.LFO")
#define TEXT_LFO_RANGE                 OMT_("LFO.Range")
#define TEXT_LFO_RATE                  OMT_("LFO.Rate")

struct acrusher_data {
	obs_source_t                  *context;

	float                          sample_rate;
	float                          sample_rate_rational;
	size_t                         channels;

	float                          level_in;
	float                          level_out;
	float                          bit_reduction;
	float                          wet_dry_mix;
	uint64_t                       mode;
	float                          dc_offset;
	float                          anti_aliasing;
	float                          sample_reduction;

	bool                           enable_lfo;
	float                          lfo_range;
	float                          lfo_rate;
};


static const char *acrusher_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("FFMPEG.acrusher");
}

static void acrusher_destroy(void *data)
{
	struct acrusher_data *ng = data;
	bfree(ng);
}

static inline float ms_to_secf(int ms)
{
	return (float)ms * .0001f;
}

static void acrusher_update(void *data, obs_data_t *settings)
{
	struct acrusher_data *ng = data;

	struct audio_output_info const *obs_current_audio_settings =
		bzalloc(sizeof(struct audio_output_info));
	obs_current_audio_settings = audio_output_get_info(obs_get_audio());

	ng->sample_rate = obs_current_audio_settings->samples_per_sec;
	ng->sample_rate_rational = 1.0f /
		(float)obs_current_audio_settings->samples_per_sec;

	ng->channels = obs_current_audio_settings->speakers;

	ng->level_in = (float)obs_data_get_double(settings, SET_LEVEL_IN);
	ng->level_out = (float)obs_data_get_double(settings, SET_LEVEL_OUT);
	ng->bit_reduction = (float)obs_data_get_int(settings, SET_BIT_REDUCTION);
	ng->wet_dry_mix = (uint64_t)obs_data_get_int(settings, SET_WET_DRY_MIX) * 0.01f;
	ng->dc_offset = (float)obs_data_get_double(settings, SET_DC_OFFSET);
	ng->anti_aliasing = (float)obs_data_get_double(settings, SET_AA);
	ng->sample_reduction = (float)obs_data_get_int(settings, SET_SAMPLE_REDUCTION);

	ng->enable_lfo = obs_data_get_bool(settings, SET_ENABLE_LFO);
	ng->lfo_range = (float)obs_data_get_double(settings, SET_LFO_RANGE);
	ng->lfo_rate = (float)obs_data_get_double(settings, SET_LFO_RATE);

	bfree(obs_current_audio_settings);
}

static void *acrusher_create(obs_data_t *settings, obs_source_t *filter)
{
	struct acrusher_data *ng = bzalloc(sizeof(*ng));
	ng->context = filter;
	acrusher_update(ng, settings);
	return ng;
}

static inline float samplereduction(ACrusherContext *s, SRContext *sr, double in)
{
	sr->samples++;
	if (sr->samples >= s->round) {
		sr->target += s->samples;
		sr->real += s->round;
		if (sr->target + s->samples >= sr->real + 1) {
			sr->last = in;
			sr->target = 0;
			sr->real = 0;
		}
		sr->samples = 0;
	}
	return sr->last;
}

static inline float add_dc(double s, double dc, double idc)
{
	return s > 0 ? s * dc : s * idc;
}

static inline float remove_dc(double s, double dc, double idc)
{
	return s > 0 ? s * idc : s * dc;
}

static inline float factor(double y, double k, double aa1, double aa)
{
	return 0.5 * (sin(M_PI * (fabs(y - k) - aa1) / aa - M_PI_2) + 1);
}

static float bitreduction(void *data, float audio)
{
	struct acrusher_data *ng = data;

	const double sqr = s->sqr;
	const double coeff = s->coeff;
	const double aa = s->aa;
	const double aa1 = s->aa1;
	double y, k;

	/* Add DC Offset first. */
	in = add_dc(in, s->dc, s->idc);

	/* Main rounding calculation depending on mode. */

	switch (s->mode) {
	case 0:
	default:
		/* Linear mode. */
		y = in * coeff;
		k = roundf(y);
		if (k - aa1 <= y && y <= k + aa1) {
			k /= coeff;
		}
		else if (y > k + aa1) {
			k = k / coeff + ((k + 1) / coeff - k / coeff) *
				factor(y, k, aa1, aa);
		}
		else {
			k = k / coeff - (k / coeff - (k - 1) / coeff) *
				factor(y, k, aa1, aa);
		}
		break;
	case 1:
		/* Logarithmic mode. */
		y = sqr * log(fabs(in)) + sqr * sqr;
		k = roundf(y);
		if (!in) {
			k = 0;
		}
		else if (k - aa1 <= y && y <= k + aa1) {
			k = in / fabs(in) * exp(k / sqr - sqr);
		}
		else if (y > k + aa1) {
			double x = exp(k / sqr - sqr);
			k = FFSIGN(in) * (x + (exp((k + 1) / sqr - sqr) - x) *
				factor(y, k, aa1, aa));
		}
		else {
			double x = exp(k / sqr - sqr);
			k = in / fabs(in) * (x - (x - exp((k - 1) / sqr - sqr)) *
				factor(y, k, aa1, aa));
		}
		break;
	}

	/* Now mix between dry and wet signal */
	k += (in - k) * s->mix;

	/* And lastly remove any DR Offset. */
	k = remove_dc(k, s->dc, s->idc);

	return k;
}

static inline float lfo_get(LFOContext *lfo)
{
	double phs = FFMIN(100., lfo->phase / FFMIN(1.99, FFMAX(0.01, lfo->pwidth)) + lfo->offset);
	double val;

	if (phs > 1)
		phs = fmod(phs, 1.);

	val = sin((phs * 360.) * M_PI / 180);

	return val * lfo->amount;
}

static void lfo_advance(LFOContext *lfo, unsigned count)
{
	lfo->phase = fabs(lfo->phase + count * lfo->freq * (1. / lfo->srate));
	if (lfo->phase >= 1.)
		lfo->phase = fmod(lfo->phase, 1.);
}

static struct obs_audio_data *acrusher_filter_audio(void *data,
		struct obs_audio_data *audio)
{
	struct acrusher_data *ng = data;

	float *adata[2] = {(float*)audio->data[0], (float*)audio->data[1]};

	for (size_t i = 0; i < audio->frames; i++) {
		float cur_level = (channels == 2)
			? fmaxf(fabsf(adata[0][i]), fabsf(adata[1][i]))
			: fabsf(adata[0][i]);

		if (cur_level > open_threshold && !ng->is_open) {
			ng->is_open = true;
		}
		if (ng->level < close_threshold && ng->is_open) {
			ng->held_time = 0.0f;
			ng->is_open = false;
		}

		ng->level = fmaxf(ng->level, cur_level) - decay_rate;

		if (ng->is_open) {
			ng->attenuation = fminf(1.0f,
					ng->attenuation + attack_rate);
		} else {
			ng->held_time += sample_rate_i;
			if (ng->held_time > hold_time) {
				ng->attenuation = fmaxf(0.0f,
						ng->attenuation - release_rate);
			}
		}

		for (size_t c = 0; c < channels; c++)
			adata[c][i] *= ng->attenuation;
	}

	return audio;
}

static void acrusher_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, SET_LEVEL_IN, 1.0f);
	obs_data_set_default_double(settings, SET_LEVEL_OUT, 1.0f);
	obs_data_set_default_int(settings, SET_BIT_REDUCTION, 8);
	obs_data_set_default_int(settings, SET_WET_DRY_MIX, 50);

	obs_data_set_default_int(settings, SET_MODE, 0);

	obs_data_set_default_double(settings, SET_DC_OFFSET, 1.0f);
	obs_data_set_default_double(settings, SET_AA, 50.0f);

	obs_data_set_default_bool(settings, SET_ENABLE_LFO, false);

	obs_data_set_default_double(settings, SET_LFO_RANGE, 20.0f);
	obs_data_set_default_double(settings, SET_LFO_RATE, 0.3f);
}

static bool lfo_changed(obs_properties_t *props, obs_property_t *p,
	obs_data_t *settings)
{
	bool enabled = obs_data_get_bool(settings, SET_ENABLE_LFO);

	obs_property_set_visible(obs_properties_get(props, SET_LFO_RANGE),
		enabled);
	obs_property_set_visible(obs_properties_get(props, SET_LFO_RATE),
		enabled);

	UNUSED_PARAMETER(p);
	return true;
}

static obs_properties_t *acrusher_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_float_slider(props, SET_LEVEL_IN, TEXT_LEVEL_IN, 0.02f, 64.0f, 0.1f);
	obs_properties_add_float_slider(props, SET_LEVEL_OUT, TEXT_LEVEL_OUT, 0.02f, 64.0f, 0.1f);
	obs_properties_add_int_slider(props, SET_BIT_REDUCTION, TEXT_BIT_REDUCTION, 1, 64, 1);
	obs_properties_add_int_slider(props, SET_WET_DRY_MIX, TEXT_WET_DRY_MIX, 0, 100, 1);

	obs_property_t *mode = obs_properties_add_list(props,
		SET_MODE, TEXT_MODE,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(mode, OMT_("Mode.Linear"), 0);
	obs_property_list_add_int(mode, OMT_("Mode.Log"), 1);

	obs_properties_add_float_slider(props, SET_DC_OFFSET, TEXT_DC_OFFSET, 0.0f, 4.0f, 0.1f);
	obs_properties_add_float_slider(props, SET_AA, TEXT_AA, 0.0f, 100.0f, 0.1f);
	obs_properties_add_int_slider(props, SET_SAMPLE_REDUCTION, TEXT_SAMPLE_REDUCTION, 1, 250, 1);

	obs_property_t *enable_lfo = obs_properties_add_bool(props, SET_ENABLE_LFO, TEXT_ENABLE_LFO);
	obs_property_set_modified_callback(enable_lfo, lfo_changed);
	obs_properties_add_float_slider(props, SET_LFO_RANGE, TEXT_LFO_RANGE, 1.0, 250.0, .01);
	obs_properties_add_float_slider(props, SET_LFO_RATE, TEXT_LFO_RATE, .01, 250.0, .01);

	UNUSED_PARAMETER(data);
	return props;
}

struct obs_source_info ffmpeg_acrusher_filter = {
	.id = "acrusher_filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_AUDIO,
	.get_name = acrusher_name,
	.create = acrusher_create,
	.destroy = acrusher_destroy,
	.update = acrusher_update,
	.filter_audio = acrusher_filter_audio,
	.get_defaults = acrusher_defaults,
	.get_properties = acrusher_properties,
};
