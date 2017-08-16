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

#define SETTING_OFFSET                  "offset"
#define SETTING_THRESHOLD_1             "threshold_1"
#define SETTING_THRESHOLD_2             "threshold_2"
#define SETTING_THRESHOLD_3             "threshold_3"
#define SETTING_THRESHOLD_4             "threshold_4"

#define OMT                             obs_module_text
#define TEXT_OFFSET                     OMT("Offset")
#define TEXT_THRESHOLD_1                OMT("Threshold.1")
#define TEXT_THRESHOLD_2                OMT("Threshold.2")
#define TEXT_THRESHOLD_3                OMT("Threshold.3")
#define TEXT_THRESHOLD_4                OMT("Threshold.4")


struct crosshatching_filter_data {
	obs_source_t                   *context;

	gs_effect_t                    *effect;

	gs_eparam_t                    *offset_param;
	gs_eparam_t                    *threshold_1_param;
	gs_eparam_t                    *threshold_2_param;
	gs_eparam_t                    *threshold_3_param;
	gs_eparam_t                    *threshold_4_param;

	uint32_t                        offset;
	float                           threshold_1;
	float                           threshold_2;
	float                           threshold_3;
	float                           threshold_4;
};


/* As the functions' namesake, this provides the user facing name
 * of your Filter. */
static const char *crosshatching_filter_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Crosshatching");
}

/* This function is called (see bottom of this file for more details
 * whenever the OBS filter interface changes. So when the user is messing
 * with a slider this function is called to update the internal settings
 * in OBS, and hence the settings being passed to the CPU/GPU. */
static void crosshatching_filter_update(void *data, obs_data_t *settings)
{
	struct crosshatching_filter_data *filter = data;
	filter->offset = (uint32_t)obs_data_get_int(settings,
			SETTING_OFFSET);
	filter->threshold_1 = (float)obs_data_get_double(settings,
			SETTING_THRESHOLD_1);
	filter->threshold_2 = (float)obs_data_get_double(settings,
			SETTING_THRESHOLD_2);
	filter->threshold_3 = (float)obs_data_get_double(settings,
			SETTING_THRESHOLD_3);
	filter->threshold_4 = (float)obs_data_get_double(settings,
			SETTING_THRESHOLD_4);
}

/* Since this is C we have to be careful when destroying/removing items from
 * OBS. Jim has added several useful functions to help keep memory leaks to
 * a minimum, and handle the destruction and construction of these filters. */
static void crosshatching_filter_destroy(void *data)
{
	struct crosshatching_filter_data *filter = data;

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
static void *crosshatching_filter_create(obs_data_t *settings, obs_source_t *context)
{
	/*
	 * Because of limitations of pre-c99 compilers, you can't create an
	 * array that doesn't have a know size at compile time. The below
	 * function calculates the size needed and allocates memory to
	 * handle the source.
	 */
	struct crosshatching_filter_data *filter =
			bzalloc(sizeof(struct crosshatching_filter_data));

	/*
	 * By default the effect file is stored in the ./data directory that
	 * your filter resides in.
	 */
	#ifdef WIN32
		char *effect_path = obs_module_file("crosshatching_filter.effect");
	#else
		char *effect_path = obs_module_file("crosshatching_filter_gl.effect");
	#endif


	filter->context = context;

	/* Here we enter the GPU drawing/shader portion of our code. */
	obs_enter_graphics();

	/* Load the shader on the GPU. */
	filter->effect = gs_effect_create_from_file(effect_path, NULL);

	/*
	 * If the filter exists grab the param names and assign them to the
	 * struct for later recall/use.
	 */
	if (filter->effect) {
		filter->offset_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_OFFSET);
		filter->threshold_1_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_THRESHOLD_1);
		filter->threshold_2_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_THRESHOLD_2);
		filter->threshold_3_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_THRESHOLD_3);
		filter->threshold_4_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_THRESHOLD_4);
	}

	obs_leave_graphics();

	bfree(effect_path);

	if (!filter->effect) {
		crosshatching_filter_destroy(filter);
		return NULL;
	}

	/*
	 * It's important to call the update function here. if we don't
	 * we could end up with the user controlled sliders and values
	 * updating, but the visuals not updating to match.
	 */
	crosshatching_filter_update(filter, settings);
	return filter;
}

/* This is where the actual rendering of the filter takes place. */
static void crosshatching_filter_render(void *data, gs_effect_t *effect)
{
	struct crosshatching_filter_data *filter = data;

	if (!obs_source_process_filter_begin(filter->context, GS_RGBA,
				OBS_ALLOW_DIRECT_RENDERING))
		return;

	/* Now pass the interface variables to the .shader file. */
	gs_effect_set_float(filter->offset_param,
			(float)filter->offset);
	gs_effect_set_float(filter->threshold_1_param,
			filter->threshold_1);
	gs_effect_set_float(filter->threshold_2_param,
			filter->threshold_2);
	gs_effect_set_float(filter->threshold_3_param,
			filter->threshold_3);
	gs_effect_set_float(filter->threshold_4_param,
			filter->threshold_4);

	obs_source_process_filter_end(filter->context, filter->effect, 0, 0);

	UNUSED_PARAMETER(effect);
}

/*
 * This function sets the interface. the types (add_*_Slider), the type of
 * data collected (int), the internal name, user-facing name, minimum,
 * maximum and step values. While a custom interface can be built, for a
 * simple filter like this it's better to use the supplied functions.
 */
static obs_properties_t *crosshatching_filter_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_int_slider(props, SETTING_OFFSET,
			TEXT_OFFSET, 1, 10, 1);
	obs_properties_add_float_slider(props, SETTING_THRESHOLD_1,
			TEXT_THRESHOLD_1, 0.01f, 1.0f, 0.01f);
	obs_properties_add_float_slider(props, SETTING_THRESHOLD_2,
			TEXT_THRESHOLD_2, 0.01f, 1.0f, 0.01f);
	obs_properties_add_float_slider(props, SETTING_THRESHOLD_3,
			TEXT_THRESHOLD_3, 0.01f, 1.0f, 0.01f);
	obs_properties_add_float_slider(props, SETTING_THRESHOLD_4,
			TEXT_THRESHOLD_4, 0.01f, 1.0f, 0.01f);

	UNUSED_PARAMETER(data);
	return props;
}

/*
 * As the functions' namesake, this provides the default settings for any
 * options you wish to provide a default for. *NOTE* this function is
 * completely optional, as is providing a default for any particular
 * option.
 */
static void crosshatching_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, SETTING_OFFSET, 5);
	obs_data_set_default_double(settings, SETTING_THRESHOLD_1, 1.0);
	obs_data_set_default_double(settings, SETTING_THRESHOLD_2, 0.7);
	obs_data_set_default_double(settings, SETTING_THRESHOLD_3, 0.5);
	obs_data_set_default_double(settings, SETTING_THRESHOLD_4, 0.3);
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
struct obs_source_info crosshatching_filter = {
	.id                            = "crosshatching_filter",
	.type                          = OBS_SOURCE_TYPE_FILTER,
	.output_flags                  = OBS_SOURCE_VIDEO,
	.get_name                      = crosshatching_filter_name,
	.create                        = crosshatching_filter_create,
	.destroy                       = crosshatching_filter_destroy,
	.video_render                  = crosshatching_filter_render,
	.update                        = crosshatching_filter_update,
	.get_properties                = crosshatching_filter_properties,
	.get_defaults                  = crosshatching_filter_defaults
};