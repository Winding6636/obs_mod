/****************************************************************************
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
#include <graphics/image-file.h>
#include <util/dstr.h>


#define SETTING_NOISE_TEX              "noise_texture"
#define SETTING_REFRESH_RATE           "refresh_rate"
#define SETTING_MOTION_RATE            "motion_rate"
#define SETTING_SIN_MOVEMENT           "sin_movement"
#define SETTING_COS_MOVEMENT           "cos_movement"
#define SETTING_LUM_THRESH             "luminance_threshold"
#define SETTING_LUM_BOOST              "luminance_boost"
#define SETTING_COLOR_AMP              "color_amplification"
#define SETTING_HVC                    "hvc"
#define SETTING_VISION_COLOR           "vision_color"


#define OMT                            obs_module_text

#define TEXT_NOISE_TEX_PATH            OMT("Noise.Texture")
#define TEXT_REFRESH_RATE              OMT("Refresh.Rate")
#define TEXT_MOTION_RATE               OMT("Motion.Rate")
#define TEXT_LUM_THRESH                OMT("Luminance.Threshold")
#define TEXT_LUM_BOOST                 OMT("Luminance.Boost")
#define TEXT_COLOR_AMP                 OMT("Color.Amplification")


struct night_vision_filter_data {
	obs_source_t                   *context;

	gs_effect_t                    *effect;

	gs_texture_t                   *target;
	gs_eparam_t                    *noise_tex_param;
	gs_image_file_t                 noise_texture;

	gs_eparam_t                    *sin_movement_param;
	gs_eparam_t                    *cos_movement_param;
	gs_eparam_t                    *lum_thresh_param;
	gs_eparam_t                    *lum_boost_param;
	gs_eparam_t                    *color_amp_param;
	gs_eparam_t                    *hvc_param;
	gs_eparam_t                    *vision_color_param;

	char                           *file;

	float                           refresh_rate;
	float                           running_time;
	float                           movement_amount;
	float                           motion_rate;
	float                           sin_movement;
	float                           cos_movement;
	float                           lum_thresh;
	float                           lum_boost;
	float                           color_amp;
	struct vec3                     hvc;
	struct vec3                     vision_color;
};


/*
 * As the functions' namesake, this provides the user facing name
 * of your Filter.
 */
static const char *night_vision_filter_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Night.Vision");
}

/*
 * This function is called (see bottom of this file for more details
 * whenever the OBS filter interface changes. So when the user is messing
 * with a slider this function is called to update the internal settings
 * in OBS, and hence the settings being passed to the CPU/GPU.
 *
 * Here we do some special stuff with textures and whatnot.
 */
static void night_vision_filter_update(void *data, obs_data_t *settings)
{
	struct night_vision_filter_data *filter = data;

	/* Get the path to the noise texture. */
	const char *path = obs_data_get_string(settings, SETTING_NOISE_TEX);

	/* Free the current data to avoid memory leaks. */
	bfree(filter->file);
	if (path)
		filter->file = bstrdup(path);

	/*
	 * Now we enter the OBS Graphics routine to free up the texture
	 * memory in use on the GPU.
	 */
	obs_enter_graphics();
	gs_image_file_free(&filter->noise_texture);
	obs_leave_graphics();

	/* Now have OBS load, prep and upload the new texture to the GPU. */
	gs_image_file_init(&filter->noise_texture, path);
	obs_enter_graphics();
	gs_image_file_init_texture(&filter->noise_texture);
	filter->target = filter->noise_texture.texture;

	/* Now grab the rest of the settings from the OBS interface. */
	filter->motion_rate = (float)obs_data_get_double(settings,
			SETTING_MOTION_RATE);
	filter->refresh_rate = (float)obs_data_get_double(settings,
			SETTING_REFRESH_RATE);
	filter->lum_thresh = (float)obs_data_get_double(settings,
			SETTING_LUM_THRESH) / 100.0f;
	filter->lum_boost = (float)obs_data_get_double(settings,
			SETTING_LUM_BOOST);
	filter->color_amp = (float)obs_data_get_double(settings,
			SETTING_COLOR_AMP);

	/*
	 * Reload the effect file to avoid the silly DX "Can't compile on
	 * the fly like GLSL can..." fun.
	 * By default the effect file is stored in the ./data directory that
	 * your filter resides in.
	 */
	char *effect_path = obs_module_file("night_vision_filter.effect");
	/* Check for the effect and log if it's missing */
	if (!effect_path) {
		blog(LOG_ERROR, "Could not find night_vision_filter.effect");
	}

	/*
	 * Destroy the current filter, then load up the new one, then free
	 * up the memory used by the path variable to keep things sane.
	 */
	gs_effect_destroy(filter->effect);
	filter->effect = gs_effect_create_from_file(effect_path, NULL);
	bfree(effect_path);

	obs_leave_graphics();
}

/*
 * Since this is C we have to be careful when destroying/removing items from
 * OBS. Jim has added several useful functions to help keep memory leaks to
 * a minimum, and handle the destruction and construction of these filters.
 */
static void night_vision_filter_destroy(void *data)
{
	struct night_vision_filter_data *filter = data;

	if (filter->effect) {
		obs_enter_graphics();
		gs_effect_destroy(filter->effect);
		/* Make sure to destroy the texture after the filter. */
		gs_image_file_free(&filter->noise_texture);
		obs_leave_graphics();
	}

	/* Now free the file (handle?) before freeing the internal data. */
	bfree(filter->file);
	bfree(data);
}

/*
 * When you apply a filter OBS creates it, and adds it to the source. OBS also
 * starts rendering it immediately. This function doesn't just 'create' the
 * filter, it also calls the render function (farther below) that contains the
 * actual rendering code.
 */
static void *night_vision_filter_create(obs_data_t *settings,
		obs_source_t *context)
{
	/*
	 * Because of limitations of pre-c99 compilers, you can't create an
	 * array that doesn't have a know size at compile time. The below
	 * function calculates the size needed and allocates memory to
	 * handle the source.
	 */
	struct night_vision_filter_data *filter =
			bzalloc(sizeof(struct night_vision_filter_data));

	filter->context = context;

	/* Assign our 'constants' values. */
	vec3_set(&filter->hvc, 0.30f, 0.59f, 0.11f);
	vec3_set(&filter->vision_color, 0.1f, 0.95f, 0.2f);

	/*
	 * It's important to call the update function here. if we don't
	 * we could end up with the user controller sliders and values
	 * updating, but the visuals not updating to match.
	 */
	obs_source_update(context, settings);
	return filter;
}

/*
 * Every frame OBS calls functions registered with the .video_tick option
 * (please see the obs_source_info struct notes for more information) OBS
 * calls any functions register
 *
 * Do try to keep what you do in here to a minimum.
 */
static void night_vision_video_tick(void *data, float seconds)
{
	struct night_vision_filter_data *filter = data;

	filter->running_time += seconds;
	filter->movement_amount = filter->running_time / filter->refresh_rate;
	filter->sin_movement = sinf(filter->movement_amount *
			filter->motion_rate) *0.4f;
	filter->cos_movement = cosf(filter->movement_amount *
			filter->motion_rate) *0.4f;
}

/* This is where the actual rendering of the filter takes place. */
static void night_vision_filter_render(void *data, gs_effect_t *effect)
{
	struct night_vision_filter_data *filter = data;
	obs_source_t *target = obs_filter_get_target(filter->context);

	/* Check if the filter should be rendered. If not, then skip it. */
	if (!target || !filter->target || !filter->effect) {
		obs_source_skip_video_filter(filter->context);
		return;
	}

	if (!obs_source_process_filter_begin(filter->context, GS_RGBA,
				OBS_ALLOW_DIRECT_RENDERING))
		return;

	/* Now pass the interface variables to the .shader file. */

	filter->noise_tex_param = gs_effect_get_param_by_name(filter->effect,
			SETTING_NOISE_TEX);
	gs_effect_set_texture(filter->noise_tex_param, filter->target);

	filter->sin_movement_param = gs_effect_get_param_by_name(
			filter->effect, SETTING_SIN_MOVEMENT);
	gs_effect_set_float(filter->sin_movement_param,
			filter->sin_movement);
	filter->cos_movement_param = gs_effect_get_param_by_name(
			filter->effect, SETTING_COS_MOVEMENT);
	gs_effect_set_float(filter->cos_movement_param,
			filter->cos_movement);

	filter->lum_thresh_param = gs_effect_get_param_by_name(
			filter->effect, SETTING_LUM_THRESH);
	gs_effect_set_float(filter->lum_thresh_param, filter->lum_thresh);

	filter->lum_boost_param = gs_effect_get_param_by_name(
			filter->effect, SETTING_LUM_BOOST);
	gs_effect_set_float(filter->lum_boost_param, filter->lum_boost);

	filter->color_amp_param = gs_effect_get_param_by_name(
			filter->effect, SETTING_COLOR_AMP);
	gs_effect_set_float(filter->color_amp_param, filter->color_amp);

	filter->hvc_param = gs_effect_get_param_by_name(
			filter->effect, SETTING_HVC);
	gs_effect_set_vec3(filter->hvc_param, &filter->hvc);

	filter->vision_color_param = gs_effect_get_param_by_name(
			filter->effect, SETTING_VISION_COLOR);
	gs_effect_set_vec3(filter->vision_color_param, &filter->vision_color);


	obs_source_process_filter_end(filter->context, filter->effect, 0, 0);

	UNUSED_PARAMETER(effect);
}

/*
 * This function sets the interface. the types (add_*_Slider), the type of
 * data collected (int), the internal name, user-facing name, minimum,
 * maximum and step values. While a custom interface can be built, for a
 * simple filter like this it's better to use the supplied functions.
 */
static obs_properties_t *night_vision_filter_properties(void *data)
{
	struct night_vision_filter_data *settings = data;
	struct dstr path = {0};
	const char *slash;

	obs_properties_t *props = obs_properties_create();
	struct dstr filter_str = {0};

	/* Setup the option to select a file and the default path. */
	dstr_cat(&filter_str, "(*.png)");

	if (settings && settings->file && *settings->file) {
		dstr_copy(&path, settings->file);
	} else {
		dstr_copy(&path, obs_module_file("noise"));
		dstr_cat_ch(&path, '/');
	}

	/* Now clean up the path for cross-platform equality. */
	dstr_replace(&path, "\\", "/");
	slash = strrchr(path.array, '/');
	if (slash)
		dstr_resize(&path, slash - path.array + 1);

	/* Add to the interface the file path option. */
	obs_properties_add_path(props, SETTING_NOISE_TEX,
			TEXT_NOISE_TEX_PATH, OBS_PATH_FILE,
			filter_str.array, path.array);
	obs_properties_add_float_slider(props, SETTING_REFRESH_RATE,
			TEXT_REFRESH_RATE, 0.01, 240.0, 0.01);
	obs_properties_add_float_slider(props, SETTING_MOTION_RATE,
			TEXT_MOTION_RATE, 1.0, 100.0, 0.1);
	obs_properties_add_float_slider(props, SETTING_LUM_THRESH,
			TEXT_LUM_THRESH, 1.0, 100.0, 0.1);
	obs_properties_add_float_slider(props, SETTING_LUM_BOOST,
			TEXT_LUM_BOOST, 0.1, 10.0, 0.1);
	obs_properties_add_float_slider(props, SETTING_COLOR_AMP,
			TEXT_COLOR_AMP, 0.1, 10.0, 0.1);

	/* Keep that memory usage/leaks/fragmentation to a minimum. */
	dstr_free(&filter_str);

	return props;
}

/*
 * As the functions' namesake, this provides the default settings for any
 * options you wish to provide a default for. *NOTE* this function is
 * completely optional, as is providing a default for any particular
 * option.
 */
static void night_vision_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, SETTING_REFRESH_RATE, 240.0);
	obs_data_set_default_double(settings, SETTING_MOTION_RATE, 5.0);
	obs_data_set_default_double(settings, SETTING_LUM_THRESH, 20.0);
	obs_data_set_default_double(settings, SETTING_LUM_BOOST, 2.0);
	obs_data_set_default_double(settings, SETTING_COLOR_AMP, 4.0);
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
 *
 * For this source, to keep track of time we have to add the .video_tick
 * to the obs_source_info struct below.
 */
struct obs_source_info night_vision_filter = {
	.id                            = "night_vision_filter",
	.type                          = OBS_SOURCE_TYPE_FILTER,
	.output_flags                  = OBS_SOURCE_VIDEO,
	.get_name                      = night_vision_filter_name,
	.create                        = night_vision_filter_create,
	.destroy                       = night_vision_filter_destroy,
	.video_render                  = night_vision_filter_render,
	.video_tick                    = night_vision_video_tick,
	.update                        = night_vision_filter_update,
	.get_properties                = night_vision_filter_properties,
	.get_defaults                  = night_vision_filter_defaults
};