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
#include "obs-filters-config.h"

#define SETTING_RADIUS                "radius"
#define SETTING_BLUR_ALPHA            "blur_alpha"
#define SETTING_TEX_X_OFFSET          "tex_x_offset"
#define SETTING_TEX_Y_OFFSET          "tex_y_offset"
#define SETTING_WEIGHT_AMOUNT         "weight_amount"

#define OMT_                           obs_module_text
#define TEXT_RADIUS                    OMT_("Radius")
#define TEXT_BLUR_ALPHA                OMT_("Blur.Alpha")

#define MAX_RADIUS                     33
#define MAX_OTHER                      (MAX_RADIUS - 1) / 2



struct gaussian_blur_filter_data {
	obs_source_t                 *context;

	gs_effect_t                  *effect;

	gs_eparam_t                  *radius_param;
	gs_eparam_t                  *blur_alpha_param;
	gs_eparam_t                  *tex_x_offset_param;
	gs_eparam_t                  *tex_y_offset_param;
	gs_eparam_t                  *weight_param;

	uint8_t                       radius;
	bool                          blur_alpha;
	uint32_t                      src_width;
	uint32_t                      src_height;

	float                         weight_amount[MAX_OTHER][4];
	float                         tex_x_offset;
	float                         tex_y_offset;
};


/*
 * As the functions' namesake, this provides the user facing name
 * of your Filter.
 */
static const char *gaussian_blur_filter_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Gaussian.Blur");
}

/* Updates texture offset and weight arrays. */
static void inline gaussian_blur_filter_texture_update(void *data)
{
	struct gaussian_blur_filter_data *filter = data;

	/* First calculate the Gaussian kernel. */
	double weight_temp[MAX_RADIUS * 2 + 1];
	double kernel = filter->radius * 2 + 1;
	double twoRadiusSquaredRecip = 1.0 / (2.0 * filter->radius * filter->radius);
	blog(LOG_INFO, "Current twoRadiusSquaredRecip is %f ", twoRadiusSquaredRecip);
	double sqrtTwoPiTimesRadiusRecip = 1.0 / (sqrt(2.0 * M_PI) * filter->radius);
	blog(LOG_INFO, "Current sqrtTwoPiTimesRadiusRecip is %f ", sqrtTwoPiTimesRadiusRecip);

	int r = -filter->radius;
	blog(LOG_INFO, "Current reverse radius is %d ", r);

	double temp_sum = 0.0;
	double Sum = 0.0;

	for (int i = 0; i < kernel; i++)
	{
		double x = r;
		x *= x;
		weight_temp[i] = sqrtTwoPiTimesRadiusRecip * exp(-x * sqrtTwoPiTimesRadiusRecip);
		r++;
		Sum = Sum + weight_temp[i];
		blog(LOG_INFO, "Pre-normalized weight amount for %d is %f ", i, weight_temp[i]);
	}
	blog(LOG_INFO, "Pre-normalized weight temp total amount is %f ", Sum);

	/* Now reverse the weight numbers in the temp array so we can filter. */
	uint8_t k = filter->radius;
	uint8_t j = 0;
	while (k > j)
	{
		float temp = weight_temp[j];
		weight_temp[j] = weight_temp[k];
		weight_temp[k] = temp;
		blog(LOG_INFO, "Reversing the weight amount for %d is %f ", j, weight_temp[j]);
		blog(LOG_INFO, "Reversing the weight amount for %d is %f ", k, weight_temp[k]);
		j++;
		k--;
	}

	/* Now turn the values into the proper final kernel amounts. */
	for (int i = 0; i < filter->radius; i++)
	{
		for (int h = 0; h < 4; h++) {
			filter->weight_amount[i][h] = 
					(float)(weight_temp[i] / Sum);
		}
		
		blog(LOG_INFO, "Normalized weight amount for %d is %f ", i, filter->weight_amount[i][0]);
		temp_sum = temp_sum + filter->weight_amount[i][0];
	}
	blog(LOG_INFO, "Normalized weight total amount is %f ", temp_sum);

	

	filter->src_width = obs_source_get_width(
			obs_filter_get_target(filter->context));
	blog(LOG_INFO, "src_width from OBS is %d", filter->src_width);
	filter->tex_x_offset = (float)(1.0 / filter->src_width);
	blog(LOG_INFO, "tex_x_offset is %f", filter->tex_x_offset);

	filter->src_height = obs_source_get_height(
			obs_filter_get_target(filter->context));
	blog(LOG_INFO, "src_height from OBS is %d", filter->src_height);
	filter->tex_y_offset = (float)(1.0 / filter->src_height);
	blog(LOG_INFO, "tex_y_offset is %f", filter->tex_y_offset);
}

/*
 * This function is called (see bottom of this file for more details)
 * whenever the OBS filter interface changes. So when the user is messing
 * with a slider this function is called to update the internal settings
 * in OBS, and hence the settings being passed to the CPU/GPU.
 */
static void gaussian_blur_filter_update(void *data, obs_data_t *settings)
{
	struct gaussian_blur_filter_data *filter = data;

	/* Now pull in all the settings from the UI. */
	filter->radius = (uint8_t)obs_data_get_int(settings,
			SETTING_RADIUS);
	filter->blur_alpha = obs_data_get_int(settings,
			SETTING_BLUR_ALPHA);

	/* Call function to calculate the array numbers. */
	gaussian_blur_filter_texture_update(filter);
}

/*
 * Since this is C we have to be careful when destroying/removing items from
 * OBS. Jim has added several useful functions to help keep memory leaks to
 * a minimum, and handle the destruction and construction of these filters.
 */
static void gaussian_blur_filter_destroy(void *data)
{
	struct gaussian_blur_filter_data *filter = data;

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
static void *gaussian_blur_filter_create(obs_data_t *settings,
		obs_source_t *context)
{
	/*
	 * Because of limitations of pre-c99 compilers, you can't create an
	 * array that doesn't have a know size at compile time. The below
	 * function calculates the size needed and allocates memory to
	 * handle the source.
	 */
	struct gaussian_blur_filter_data *filter =
			bzalloc(sizeof(struct gaussian_blur_filter_data));

	filter->context = context;

	for (size_t i = 0; i < MAX_OTHER; i++) {
		for (size_t j = 0; i < 4; i++) {
			filter->weight_amount[i][j] = 1.0f;
		}
	}

	filter->tex_x_offset = 1.0f;
	filter->tex_y_offset = 1.0f;
	filter->src_height = 1;
	filter->src_width = 1;

	/* Here we enter the GPU drawing/shader portion of our code. */
	obs_enter_graphics();

	/*
	 * Reload the effect file to avoid the silly DX "Can't compile on
	 * the fly like GLSL can..." fun.
	 * By default the effect file is stored in the ./data directory that
	 * your filter resides in.
	 */
	char *effect_path = obs_module_file("gaussian_blur_filter.effect");

	/* Check for the effect and log if it's missing */
	if (!effect_path) {
		blog(LOG_ERROR, "Could not find gaussian_blur_filter.effect");
		return NULL;
	}

	/* Load the shader on the GPU. */
	filter->effect = gs_effect_create_from_file(effect_path, NULL);

	/* If the filter is active pass the parameters to the filter. */
	if (filter->effect) {
		filter->radius_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_RADIUS);
		filter->blur_alpha_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_BLUR_ALPHA);
		filter->tex_x_offset_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_TEX_X_OFFSET);
		filter->tex_y_offset_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_TEX_Y_OFFSET);
		filter->weight_param = gs_effect_get_param_by_name(
				filter->effect, SETTING_WEIGHT_AMOUNT);
	}

	obs_leave_graphics();

	bfree(effect_path);

	if (!filter->effect) {
		gaussian_blur_filter_destroy(filter);
		return NULL;
	}

	/*
	 * It's important to call the update function here. if we don't
	 * we could end up with the user controlled sliders and values
	 * updating, but the visuals not updating to match.
	 */
	gaussian_blur_filter_update(filter, settings);

	return filter;
}

/* This is where the actual rendering of the filter takes place. */
static void gaussian_blur_filter_render(void *data, gs_effect_t *effect)
{
	struct gaussian_blur_filter_data *filter = data;
	obs_source_t *target = obs_filter_get_target(filter->context);

	/* Check if the filter should be rendered. If not, then skip it. */
	if (!obs_source_process_filter_begin(filter->context, GS_RGBA,
			OBS_ALLOW_DIRECT_RENDERING))
		return;

	/*
	 * Now that we know the filter is active and good, calculate the
	 * height and width of the source if it has changed.
	 */

	if (obs_source_get_width(
		obs_filter_get_target(filter->context)) != filter->src_width || 
		obs_source_get_height(
			obs_filter_get_target(filter->context)) != filter->src_height) {
		filter->src_width = obs_source_get_width(
			obs_filter_get_target(filter->context));
		filter->tex_x_offset = (float)(1.0 / filter->src_width);

		filter->src_height = obs_source_get_height(
			obs_filter_get_target(filter->context));
		filter->tex_y_offset = (float)(1.0 / filter->src_height);

		gaussian_blur_filter_texture_update(filter);
	}

	/* Now pass variables to the .shader file. */
	gs_effect_set_int(filter->radius_param, filter->radius);
	gs_effect_set_bool(filter->blur_alpha_param, filter->blur_alpha);
	gs_effect_set_float(filter->tex_x_offset_param, filter->tex_x_offset);
	gs_effect_set_float(filter->tex_y_offset_param, filter->tex_y_offset);
	gs_effect_set_val(filter->weight_param, filter->weight_amount,
			sizeof(float) * MAX_RADIUS * 4);

	obs_source_process_filter_end(filter->context, filter->effect, 0, 0);

	UNUSED_PARAMETER(effect);
}

/*
 * This function sets the interface. the types (add_*_Slider), the type of
 * data collected (int), the internal name, user-facing name, minimum,
 * maximum and step values. While a custom interface can be built, for a
 * simple filter like this it's better to use the supplied functions.
 */
static obs_properties_t *gaussian_blur_filter_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_int_slider(props, SETTING_RADIUS,
			TEXT_RADIUS, 3, MAX_RADIUS, 1);
	obs_properties_add_bool(props, SETTING_BLUR_ALPHA, TEXT_BLUR_ALPHA);

	UNUSED_PARAMETER(data);
	return props;
}

/*
 * As the functions' namesake, this provides the default settings for any
 * options you wish to provide a default for. *NOTE* this function is
 * completely optional, as is providing a default far any particular
 * option.
 */
static void gaussian_blur_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, SETTING_RADIUS, 29);
	obs_data_set_default_bool(settings, SETTING_BLUR_ALPHA, FALSE);
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
struct obs_source_info gaussian_blur_filter = {
	.id                          = "gaussian_blur_filter",
	.type                        = OBS_SOURCE_TYPE_FILTER,
	.output_flags                = OBS_SOURCE_VIDEO,
	.get_name                    = gaussian_blur_filter_name,
	.create                      = gaussian_blur_filter_create,
	.destroy                     = gaussian_blur_filter_destroy,
	.video_render                = gaussian_blur_filter_render,
	.update                      = gaussian_blur_filter_update,
	.get_properties              = gaussian_blur_filter_properties,
	.get_defaults                = gaussian_blur_filter_defaults
};
