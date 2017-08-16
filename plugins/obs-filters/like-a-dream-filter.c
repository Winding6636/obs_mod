/*****************************************************************************
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
*****************************************************************************/

#include <obs-module.h>

#define SETTING_GAMMA                  "gamma"
#define SETTING_NUMBER_OF_SHIFTS       "number_of_shifts"
#define SETTING_SHIFT_GAP              "shift_gap"
#define SETTING_OPACITY                "opacity"
#define SETTING_TINT                   "tint"
#define SETTING_TINT_RANGE             "tint_range"

#define OMT                            obs_module_text
#define TEXT_GAMMA                     OMT("Gamma")
#define TEXT_NUMBER_OF_SHIFTS          OMT("Number.of.Shifts")
#define TEXT_SHIFT_GAP                 OMT("Shift.Gap")
#define TEXT_OPACITY                   OMT("Opacity")
#define TEXT_TINT                      OMT("Tint")
#define TEXT_TINT_RANGE                OMT("Tint.Range")


struct like_a_dream_filter_data {
	obs_source_t                   *context;

	gs_effect_t                    *effect;

	gs_eparam_t                    *gamma_param;
	gs_eparam_t                    *number_of_shifts_param;
	gs_eparam_t                    *shift_gap_param;
	gs_eparam_t                    *opacity_param;
	gs_eparam_t                    *tint_param;
	gs_eparam_t                    *tint_range_param;

	struct vec3                     gamma;
	uint8_t                         number_of_shifts;
	float                           shift_gap;
	float                           opacity;
	struct vec4                     tint;
	float                           tint_range;
};

/*
 * As the functions' namesake, this provides the user facing name
 * of your Filter.
 */
static const char *like_a_dream_filter_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Like.Dream");
}

/*
 * This function is called (see bottom of this file for more details
 * whenever the OBS filter interface changes. So when the user is messing
 * with a slider this function is called to update the internal settings
 * in OBS, and hence the settings being passed to the CPU/GPU.
 */
static void like_a_dream_filter_update(void *data, obs_data_t *settings)
{
	struct like_a_dream_filter_data *filter = data;

	/* Build our Gamma numbers. */
	double gamma = obs_data_get_double(settings, SETTING_GAMMA);
	gamma = (gamma < 0.0) ? (-gamma + 1.0) : (1.0 / (gamma + 1.0));
	vec3_set(&filter->gamma, (float)gamma, (float)gamma, (float)gamma);

	filter->number_of_shifts = (uint8_t)obs_data_get_int(settings,
			SETTING_NUMBER_OF_SHIFTS);
	filter->shift_gap = (float)(obs_data_get_int(settings,
			SETTING_SHIFT_GAP) * 0.0001f);

	filter->opacity = (float)obs_data_get_double(settings,
			SETTING_OPACITY) * 0.01f;

	/* Now get the overlay tint data. */
	uint32_t tint = (uint32_t)obs_data_get_int(settings, SETTING_TINT);
	vec4_from_rgba(&filter->tint, tint);
	filter->tint_range = (float)obs_data_get_double(settings,
			SETTING_TINT_RANGE) * 0.1f;
}

/*
 * Since this is C we have to be careful when destroying/removing items from
 * OBS. Jim has added several useful functions to help keep memory leaks to
 * a minimum, and handle the destruction and construction of these filters.
 */
static void like_a_dream_filter_destroy(void *data)
{
	struct like_a_dream_filter_data *filter = data;

	if (filter->effect) {
		obs_enter_graphics();
		gs_effect_destroy(filter->effect);
		obs_leave_graphics();
	}

	bfree(data);
}

/*
 * When you apply a filter OBS creates it, and adds it to the source. OBS also
 * starts rendering it immediately. This function doesn't just 'create' the
 * filter, it also calls the render function (farther below) that contains the
 * actual rendering code.
 */
static void *like_a_dream_filter_create(obs_data_t *settings,
		obs_source_t *context)
{
	/*
	 * Because of limitations of pre-c99 compilers, you can't create an
	 * array that doesn't have a know size at compile time. The below
	 * function calculates the size needed and allocates memory to
	 * handle the source.
	 */
	struct like_a_dream_filter_data *filter =
			bzalloc(sizeof(struct like_a_dream_filter_data));


	/*
	 * By default the effect file is stored in the ./data directory that
	 * your filter resides in.
	 */
	char *effect_path = obs_module_file("like_a_dream_filter.effect");

	filter->context = context;

	/* Check for the effect and log if it's missing */
	if (!effect_path) {
		blog(LOG_ERROR, "Could not find like_a_dream_filter.effect");
		return NULL;
	}

	/* Here we enter the GPU drawing/shader portion of our code. */
	obs_enter_graphics();

	/* Load the shader on the GPU. */
	filter->effect = gs_effect_create_from_file(effect_path, NULL);

	/* If the filter is active pass the parameters to the filter. */
	if (filter->effect) {
		filter->gamma_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_GAMMA);
		filter->number_of_shifts_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_NUMBER_OF_SHIFTS);
		filter->shift_gap_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_SHIFT_GAP);
		filter->opacity_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_OPACITY);
		filter->tint_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_TINT);
		filter->tint_range_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_TINT_RANGE);
	}

	obs_leave_graphics();

	bfree(effect_path);

	if (!filter->effect) {
		like_a_dream_filter_destroy(filter);
		return NULL;
	}

	/*
	 * It's important to call the update function here. if we don't
	 * we could end up with the user controlled sliders and values
	 * updating, but the visuals not updating to match.
	 */
	like_a_dream_filter_update(filter, settings);
	return filter;
}

/* This is where the actual rendering of the filter takes place. */
static void like_a_dream_filter_render(void *data, gs_effect_t *effect)
{
	struct like_a_dream_filter_data *filter = data;

	if (!obs_source_process_filter_begin(filter->context, GS_RGBA,
			OBS_ALLOW_DIRECT_RENDERING))
		return;

	/* Now pass the interface variables to the .shader file. */

	/*
	 * Even though the "number_of_shifts" & shift_gap variables are
	 * an int, the shader needs a float. So we call the float
	 * version of gs_effect_set.
	 */
	gs_effect_set_vec3(filter->gamma_param, &filter->gamma);
	gs_effect_set_float(filter->number_of_shifts_param,
			filter->number_of_shifts);
	gs_effect_set_float(filter->shift_gap_param, filter->shift_gap);
	gs_effect_set_float(filter->opacity_param, filter->opacity);
	gs_effect_set_vec4(filter->tint_param, &filter->tint);
	gs_effect_set_float(filter->tint_range_param, filter->tint_range);

	obs_source_process_filter_end(filter->context, filter->effect, 0, 0);

	UNUSED_PARAMETER(effect);
}

/*
 * This function sets the interface. the types (add_*_Slider), the type of
 * data collected (int), the internal name, user-facing name, minimum,
 * maximum and step values. While a custom interface can be built, for a
 * simple filter like this it's better to use the supplied functions.
 */
static obs_properties_t *like_a_dream_filter_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_float_slider(props, SETTING_GAMMA,
			TEXT_GAMMA, -3.0f, 3.0f, 0.01f);
	obs_properties_add_int_slider(props, SETTING_NUMBER_OF_SHIFTS,
			TEXT_NUMBER_OF_SHIFTS, 1, 100, 1);
	obs_properties_add_int_slider(props, SETTING_SHIFT_GAP,
			TEXT_SHIFT_GAP, 1, 100, 1);
	obs_properties_add_float_slider(props, SETTING_OPACITY,
			TEXT_OPACITY, 0.0f, 100.0f, 0.1f);
	obs_properties_add_color(props, SETTING_TINT, TEXT_TINT);
	obs_properties_add_float_slider(props, SETTING_TINT_RANGE,
			TEXT_TINT_RANGE, -2.0f, 4.0f, 0.01f);

	UNUSED_PARAMETER(data);
	return props;
}

/*
 * As the functions' namesake, this provides the default settings for any
 * options you wish to provide a default for. *NOTE* this function is
 * completely optional, as is providing a default for any particular
 * option.
 */
static void like_a_dream_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, SETTING_GAMMA, 0.0);
	obs_data_set_default_int(settings, SETTING_NUMBER_OF_SHIFTS, 6);
	obs_data_set_default_int(settings, SETTING_SHIFT_GAP, 9);
	obs_data_set_default_double(settings, SETTING_OPACITY, 100.0);
	obs_data_set_default_int(settings, SETTING_TINT, 0xFFFFFF);
	obs_data_set_default_double(settings, SETTING_TINT_RANGE, 1.0);
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
struct obs_source_info like_a_dream_filter = {
	.id                            = "like_a_dream_filter",
	.type                          = OBS_SOURCE_TYPE_FILTER,
	.output_flags                  = OBS_SOURCE_VIDEO,
	.get_name                      = like_a_dream_filter_name,
	.create                        = like_a_dream_filter_create,
	.destroy                       = like_a_dream_filter_destroy,
	.video_render                  = like_a_dream_filter_render,
	.update                        = like_a_dream_filter_update,
	.get_properties                = like_a_dream_filter_properties,
	.get_defaults                  = like_a_dream_filter_defaults
};