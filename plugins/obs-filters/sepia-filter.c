/*****************************************************************************
Copyright (C) 2016 by c3r1c3 <c3r1c3@nevermindonline.com>

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
#include <graphics/matrix4.h>

#define SETTING_GAMMA                  "gamma"
#define SETTING_OPACITY                "opacity"
#define SETTING_INTENSITY              "intensity"
#define SETTING_RED_CHANNEL            "red_channel"
#define SETTING_GREEN_CHANNEL          "green_channel"

#define OMT                            obs_module_text
#define TEXT_GAMMA                     OMT("Sepia.Gamma")
#define TEXT_OPACITY                   OMT("Sepia.Opacity")
#define TEXT_INTENSITY                 OMT("Sepia.Intensity")
#define TEXT_RED_CHANNEL               OMT("Sepia.RedChannel")
#define TEXT_GREEN_CHANNEL             OMT("Sepia.GreenChannel")

struct sepia_filter_data {
	obs_source_t                   *context;

	gs_effect_t                    *effect;

	gs_eparam_t                    *gamma_param;
	gs_eparam_t                    *intensity_vec_param;
	gs_eparam_t                    *sepia_matrix_param;

	struct vec3                     gamma;
	float                           opacity;
	struct vec4                     intensity_vec;
	float                           intensity;
	float                           red_channel;
	float                           green_channel;

	/* Pre-Computes */
	struct matrix4                  sepia_matrix;
};


/*
 * As the function's name implies, this provides the user facing name
 * of your filter.
 */
static const char *sepia_filter_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("SepiaTone");
}

/*
 * This function is called (see bottom of this file for more details)
 * whenever the OBS filter interface changes. So when the user is messing
 * with a slider this function is called to update the internal settings
 * in OBS, and hence the settings being passed to the CPU/GPU.
 */
static void sepia_filter_update(void *data, obs_data_t *settings)
{
	struct sepia_filter_data *filter = data;


	/* Build our Gamma numbers. */
	double gamma = obs_data_get_double(settings, SETTING_GAMMA);
	gamma = (gamma < 0.0) ? (-gamma + 1.0) : (1.0 / (gamma + 1.0));
	vec3_set(&filter->gamma, (float)gamma, (float)gamma, (float)gamma);

	/* Build our Opacity number. */
	filter->opacity = (float)obs_data_get_double(settings,
			SETTING_OPACITY) * 0.01f;

	/* Build our Intensity numbers. */
	filter->intensity = (float)obs_data_get_double(settings,
			SETTING_INTENSITY);
	vec4_set(&filter->intensity_vec, filter->intensity,
			filter->intensity, filter->intensity, 1.0f);

	/* Grab our Red and Green Channel Settings. */
	filter->red_channel = (float)obs_data_get_double(settings,
			SETTING_RED_CHANNEL) * 0.1f;
	filter->green_channel = (float)obs_data_get_double(settings,
			SETTING_GREEN_CHANNEL) *0.1f;

	filter->sepia_matrix = (struct matrix4)
	{
		{0.3588f + (filter->red_channel),
		 0.7044f,
		 0.1368f,
		 0},

		{0.2990f,
		 0.5870f + (filter->green_channel),
		 0.1140f,
		 0},

		{0.2392f,
		 0.4696f,
		 0.0912f,
		 0},

		{0, 0, 0, filter->opacity},
	};
}

/*
 * Since this is C we have to be careful when destroying/removing items from
 * OBS. Jim has added several useful functions to help keep memory leaks to
 * a minimum, and handle the destruction and construction of these filters.
 */
static void sepia_filter_destroy(void *data)
{
	struct sepia_filter_data *filter = data;

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
static void *sepia_filter_create(obs_data_t *settings, obs_source_t *context)
{
	/*
	 * Because of limitations of pre-C99 compilers, you can't create an
	 * array that doesn't have a know size at compile time. The below
	 * function calculates the size needed and allocates memory to
	 * handle the source.
	 */
	struct sepia_filter_data *filter =
			bzalloc(sizeof(struct sepia_filter_data));

	/*
	 * By default the effect file is stored in the ./data directory that
	 * your filter resides in.
	 */
	char *effect_path = obs_module_file("sepia_filter.effect");

	filter->context = context;

	/* Check for the effect and log if it's missing */
	if (!effect_path) {
		blog(LOG_ERROR, "Could not find sepia_filter.effect");
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
		filter->intensity_vec_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_INTENSITY);
		filter->sepia_matrix_param = gs_effect_get_param_by_name(
				filter->effect, "sepia_matrix");
	}

	obs_leave_graphics();

	bfree(effect_path);

	if (!filter->effect) {
		sepia_filter_destroy(filter);
		return NULL;
	}

	/*
	 * It's important to call the update function here. if we don't
	 * we could end up with the user controlled sliders and values
	 * updating, but the visuals not updating to match.
	 */
	sepia_filter_update(filter, settings);
	return filter;
}

/* This is where the actual rendering of the filter takes place. */
static void sepia_filter_render(void *data, gs_effect_t *effect)
{
	struct sepia_filter_data *filter = data;

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
	gs_effect_set_vec4(filter->intensity_vec_param,
			&filter->intensity_vec);
	gs_effect_set_matrix4(filter->sepia_matrix_param,
			&filter->sepia_matrix);

	obs_source_process_filter_end(filter->context, filter->effect, 0, 0);

	UNUSED_PARAMETER(effect);
}

/*
 * This function sets the interface. the types (add_*_Slider), the type of
 * data collected (int), the internal name, user-facing name, minimum,
 * maximum and step values. While a custom interface can be built, for a
 * simple filter like this it's better to use the supplied functions.
 */
static obs_properties_t *sepia_filter_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_float_slider(props, SETTING_GAMMA,
			TEXT_GAMMA, -3.0f, 3.0f, 0.01f);
	obs_properties_add_float_slider(props, SETTING_OPACITY,
			TEXT_OPACITY, 0.0f, 100.0f, 0.1f);
	obs_properties_add_float_slider(props, SETTING_INTENSITY,
			TEXT_INTENSITY, 0.0f, 3.0f, 0.01f);
	obs_properties_add_float_slider(props, SETTING_RED_CHANNEL,
			TEXT_RED_CHANNEL, -5.0f, 5.0f, 0.01f);
	obs_properties_add_float_slider(props, SETTING_GREEN_CHANNEL,
			TEXT_GREEN_CHANNEL, -5.0f, 5.0f, 0.01f);

	UNUSED_PARAMETER(data);
	return props;
}

/*
 * As the function's name implies, this provides the default settings for any
 * options you wish to provide a default for. *NOTE* this function is
 * completely optional, as is providing a default for any particular
 * option.
 */
static void sepia_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, SETTING_GAMMA, 0.0);
	obs_data_set_default_double(settings, SETTING_OPACITY, 100.0);
	obs_data_set_default_double(settings, SETTING_INTENSITY, 1.0);
	obs_data_set_default_double(settings, SETTING_RED_CHANNEL, 0.0);
	obs_data_set_default_double(settings, SETTING_GREEN_CHANNEL, 0.0);
}

/*
 * So how does OBS keep track of all these plug-ins/filters? How does OBS know
 * which function to call when it needs to update a setting? Or a source? Or
 * what type of source this is?
 *
 * OBS does it through the obs_source_info struct. Notice how variables are
 * assigned the name of a function? Notice how the function name has the
 * variable name in it? While not mandatory, it helps a ton for you (and those
 * reading your code) to follow this convention.
 */
struct obs_source_info sepia_filter = {
	.id                            = "sepia_filter",
	.type                          = OBS_SOURCE_TYPE_FILTER,
	.output_flags                  = OBS_SOURCE_VIDEO,
	.get_name                      = sepia_filter_name,
	.create                        = sepia_filter_create,
	.destroy                       = sepia_filter_destroy,
	.video_render                  = sepia_filter_render,
	.update                        = sepia_filter_update,
	.get_properties                = sepia_filter_properties,
	.get_defaults                  = sepia_filter_defaults
};
