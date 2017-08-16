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

#define SETTING_NUMBER_OF_REPEATS       "number_of_repeats"
#define SETTING_DOWNSCALE_AMOUNT        "downscale_amount"
#define SETTING_DOWNSCALE_INVERSION     "downscale_inversion"
#define SETTING_X_OFFSET                "x_offset"
#define SETTING_Y_OFFSET                "y_offset"

#define OMT                             obs_module_text
#define TEXT_NUMBER_OF_REPEATS          OMT("No.Of.Repeats")
#define TEXT_DOWNSCALE_AMOUNT           OMT("Downscale.Per")
#define TEXT_X_OFFSET                   OMT("X.Offset")
#define TEXT_Y_OFFSET                   OMT("Y.Offset")


struct droste_filter_data {
	obs_source_t                   *context;

	gs_effect_t                    *effect;

	gs_eparam_t                    *number_of_repeats_param;
	gs_eparam_t                    *downscale_amount_param;
	gs_eparam_t                    *downscale_inversion_param;
	gs_eparam_t                    *x_offset_param;
	gs_eparam_t                    *y_offset_param;

	uint8_t                         number_of_repeats;
	uint8_t                         downscale_amount;
	float                           downscale_inversion;
	float                           x_offset;
	float                           y_offset;
};


/* As the functions' namesake, this provides the user facing name
 * of your Filter. */
static const char *droste_filter_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return OMT("Droste.Effect");
}

/* This function is called (see bottom of this file for more details
 * whenever the OBS filter interface changes. So when the user is messing
 * with a slider this function is called to update the internal settings
 * in OBS, and hence the settings being passed to the CPU/GPU. */
static void droste_filter_update(void *data, obs_data_t *settings)
{
	struct droste_filter_data *filter = data;
	filter->number_of_repeats = (uint8_t)obs_data_get_int(settings,
			SETTING_NUMBER_OF_REPEATS);
	filter->downscale_amount = (uint8_t)obs_data_get_int(settings,
			SETTING_DOWNSCALE_AMOUNT);
	filter->downscale_inversion = ((float)obs_data_get_int(settings,
			SETTING_DOWNSCALE_AMOUNT) * 0.01f);
	filter->x_offset = (float)obs_data_get_double(settings,
			SETTING_X_OFFSET);
	filter->y_offset = (float)obs_data_get_double(settings,
			SETTING_Y_OFFSET);
}

/* Since this is C we have to be careful when destroying/removing items from
 * OBS. Jim has added several useful functions to help keep memory leaks to
 * a minimum, and handle the destruction and construction of these filters. */
static void droste_filter_destroy(void *data)
{
	struct droste_filter_data *filter = data;

	if (filter->effect) {
		obs_enter_graphics();
		gs_effect_destroy(filter->effect);
		obs_leave_graphics();
	}

	bfree(data);
}

/* When you apply a filter OBS creates it, and adds it to the source. OBS also
 * starts rendering it immediately. This function doesn't just 'create' the
 * filter, it also calls the render function (farther below) that contains the
 * actual rendering code. */
static void *droste_filter_create(obs_data_t *settings, obs_source_t *context)
{
	/* Because of limitations of pre-C99 compilers, you can't create an
	 * array that doesn't have a know size at compile time. The below
	 * function calculates the size needed and allocates memory to
	 * handle the source. */
	struct droste_filter_data *filter =
			bzalloc(sizeof(struct droste_filter_data));

	/* By default the effect file is stored in the ./data directory that
	 * your filter resides in. */
	char *effect_path = obs_module_file("droste_filter.shader");

	filter->context = context;

	/* Here we enter the GPU drawing/shader portion of our code. */
	obs_enter_graphics();

	/* Load the shader on the GPU. */
	filter->effect = gs_effect_create_from_file(effect_path, NULL);

	/* If the filter is active pass the parameters to the filter. */
	if (filter->effect) {
		filter->number_of_repeats_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_NUMBER_OF_REPEATS);
		filter->downscale_amount_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_DOWNSCALE_AMOUNT);
		filter->downscale_inversion_param =
				gs_effect_get_param_by_name(
				filter->effect, SETTING_DOWNSCALE_INVERSION);
		filter->x_offset_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_X_OFFSET);
		filter->y_offset_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_Y_OFFSET);
	}

	obs_leave_graphics();

	bfree(effect_path);

	if (!filter->effect) {
		droste_filter_destroy(filter);
		return NULL;
	}

	/* It's important to call the update function here. if we don't
	 * we could end up with the user controlled sliders and values
	 * updating, but the visuals not updating to match. */
	droste_filter_update(filter, settings);
	return filter;
}

/* This is where the actual rendering of the filter takes place. */
static void droste_filter_render(void *data, gs_effect_t *effect)
{
	struct droste_filter_data *filter = data;

	if (!obs_source_process_filter_begin(filter->context, GS_RGBA,
			OBS_ALLOW_DIRECT_RENDERING))
		return;

	/* Now pass the interface variables to the .shader file. */
	gs_effect_set_float(filter->number_of_repeats_param,
			(float)(filter->number_of_repeats));
	gs_effect_set_float(filter->downscale_amount_param,
			(float)(1.0 - (filter->downscale_amount *
			0.01f)) - 1.0f);
	gs_effect_set_float(filter->downscale_inversion_param,
			(filter->downscale_inversion));
	gs_effect_set_float(filter->x_offset_param, (0.5f *
			(filter->downscale_inversion -
			(filter->downscale_inversion *
			(filter->x_offset * (-0.01f))))));
	gs_effect_set_float(filter->y_offset_param, (0.5f *
			(filter->downscale_inversion -
			(filter->downscale_inversion *
			(filter->y_offset * (-0.01f))))));

	obs_source_process_filter_end(filter->context, filter->effect, 0, 0);

	UNUSED_PARAMETER(effect);
}

/* This function sets the interface, the types (add_*_Slider), the type of
 * data collected (int), the internal name, user-facing name, minimum,
 * maximum and step values. While a custom interface can be built, for a
 * simple filter like this it's better to use the supplied functions.*/
static obs_properties_t *droste_filter_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_int_slider(props, SETTING_NUMBER_OF_REPEATS,
			TEXT_NUMBER_OF_REPEATS, 1, 35, 1);
	obs_properties_add_int_slider(props, SETTING_DOWNSCALE_AMOUNT,
			TEXT_DOWNSCALE_AMOUNT, 3, 50, 1);
	obs_properties_add_float_slider(props, SETTING_X_OFFSET,
			TEXT_X_OFFSET, -100.0, 100.0, 0.1);
	obs_properties_add_float_slider(props, SETTING_Y_OFFSET,
			TEXT_Y_OFFSET, -100.0, 100.0, 0.1);

	UNUSED_PARAMETER(data);
	return props;
}

/* As the functions' namesake, this provides the default settings for any
 * options you wish to provide a default for. *NOTE* this function is
 * completely optional, as is providing a default for any particular
 * option. */
static void droste_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, SETTING_NUMBER_OF_REPEATS, 8);
	obs_data_set_default_int(settings, SETTING_DOWNSCALE_AMOUNT, 8);
	obs_data_set_default_double(settings, SETTING_X_OFFSET, 0.0);
	obs_data_set_default_double(settings, SETTING_Y_OFFSET, 0.0);
}

/* So how does OBS keep track of all these plug-ins/filters? How does OBS know
 * which function to call when it needs to update a setting? Or a source? Or
 * what type of source this is?
 *
 * OBS does it through the obs_source_info_struct. Notice how variables are
 * assigned the name of a function? Notice how the function name has the
 * variable name in it? While not mandatory, it helps a ton for you (and those
 * reading your code) to follow this convention.*/
struct obs_source_info droste_filter = {
	.id                            = "droste_filter",
	.type                          = OBS_SOURCE_TYPE_FILTER,
	.output_flags                  = OBS_SOURCE_VIDEO,
	.get_name                      = droste_filter_name,
	.create                        = droste_filter_create,
	.destroy                       = droste_filter_destroy,
	.video_render                  = droste_filter_render,
	.update                        = droste_filter_update,
	.get_properties                = droste_filter_properties,
	.get_defaults                  = droste_filter_defaults
};