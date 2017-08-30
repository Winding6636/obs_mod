#include <obs-module.h>
#include <graphics/vec4.h>
#include <stdlib.h>
#include <util/threading.h>
#include <util/platform.h>
#include <obs.h>

#define SETTING_WIDTH                  "width"
#define SETTING_HEIGHT                 "height"
#define SETTING_MASTER_OPACITY         "master opacity"
#define SETTING_POSTFX                 "postfx"
#define SETTING_LAYER_COUNT            "layer count"

#define SETTING_RED                    "red"
#define SETTING_GREEN                  "green"
#define SETTING_BLUE                   "blue"
#define SETTING_OPACITY                "layer opacity"
#define SETTING_LAYER_TYPE             "layer type"

#define SETTING_GRADIENT_TYPE          "gradient type"
#define SETTING_GRADIENT_POLE_1        "gradient pole 1"
#define SETTING_GRADIENT_POLE_2        "gradient pole 2"
#define SETTING_GRADIENT_POLE_3        "gradient pole 3"

#define SETTING_NOISE_TYPE             "noise type"

#define TEXT_WIDTH                     obs_module_text("Width")
#define TEXT_HEIGHT                    obs_module_text("Height")
#define TEXT_MASTER_OPACITY            obs_module_text("Master Opacity")
#define TEXT_POSTFX                    obs_module_text("PostFX")
#define TEXT_LAYER_COUNT               obs_module_text("Layer Count")

#define TEXT_RED                       obs_module_text("Red")
#define TEXT_GREEN                     obs_module_text("Green")
#define TEXT_BLUE                      obs_module_text("Blue")
#define TEXT_OPACITY                   obs_module_text("Layer Opacity")
#define TEXT_LAYER_TYPE                obs_module_text("Layer Type")

#define TEXT_GRADIENT_TYPE             obs_module_text("Gradient Type")
#define TEXT_GRADIENT_POLE_1           obs_module_text("Gradient Pole 1")
#define TEXT_GRADIENT_POLE_2           obs_module_text("Gradient Pole 2")
#define TEXT_GRADIENT_POLE_3           obs_module_text("Gradient Pole 3")

#define TEXT_NOISE_TYPE                obs_module_text("Noise Type")


struct bgtexture_master {
	obs_source_t                   *context;

	gs_eparam_t                    *height_param;
	gs_eparam_t                    *width_param;
	gs_eparam_t                    *master_opacity_param;
	gs_eparam_t                    *postfx_param;
	gs_eparam_t                    *layer_count_param;

	double                         master_opacity;
	uint16_t                       width;
	uint16_t                       height;
};

struct bgtexture_color_selector {
	//obs_source_t                   *context;

	gs_eparam_t                    *red_param;
	gs_eparam_t                    *green_param;
	gs_eparam_t                    *blue_param;
	gs_eparam_t                    *opacity_param;

	double                         red;
	double                         green;
	double                         blue;
	double                         opacity;
};

struct bgtexture_solid {
	//obs_source_t                   *context;
	bool                           enabled;
};

struct bgtexture_gradient {
	//obs_source_t                   *context;
	bool                           enabled;

	gs_eparam_t                    *number_of_colors_param;
	gs_eparam_t                    *gradient_type;
	gs_eparam_t                    *gradient_pole_1_param;
	gs_eparam_t                    *gradient_pole_2_param;
	gs_eparam_t                    *gradient_pole_3_param;

	uint8_t                        number_of_colors;
	uint8_t                        gradient_pole_1;
	uint8_t                        gradient_pole_2;
	uint8_t                        gradient_pole_3;
};

struct bgtexture_source {
	obs_source_t *source;
	os_event_t   *stop_signal;
	pthread_t    thread;
	bool         initialized;
};

/* This tells OBS that your code is a Module. */
OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("Background Texture", "en-US")
obs_property_t;

/* This is what provides the name that shows up in the
"Add->Sources" option. */
static const char *bgtexture_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Background Texture";
}

static void draw_solid(void *data, void *bgtexture_data)
{
	/*struct bgtexture_data *current_bgtexture_data = bgtexture_data;

	uint16_t x = 0, y = 0;
	uint16_t width = current_bgtexture_data->width;
	uint16_t height = current_bgtexture_data->height;

	float *pixels = data;*/
	//float alloc_size = width * height * 4 * 32;

	/* If the texture already exists do a realloc. */
	//if ()
	//	void* realloc(void* ptr, uint32_t alloc_size);
	//else
	/* Else do a malloc. */
	//	void* malloc(uint32_t alloc_size);

	/* Now calculate the color of the solid texture. */
	//GL_RGBA32F;


	/*for (y = 0; y < height; y++) {
		for (x = 0; x < width; x = 4) {
			pixels[y + x] = current_bgtexture_data->red;
			pixels[y + x + 1] = current_bgtexture_data->green;
			pixels[y + x + 2] = current_bgtexture_data->blue;
			pixels[y + x + 3] = current_bgtexture_data->opacity;
		}
	}*/
}

static void draw_gradient(void *data, void *bgtexture_data)
{
	/*struct bgtexture_data *current_bgtexture_data = bgtexture_data;

	uint16_t x = 0, y = 0;
	uint16_t width = current_bgtexture_data->width;
	uint16_t height = current_bgtexture_data->height;

	float *pixels = data;*/
	//float alloc_size = width * height * 4 * 32;

	/* If the texture already exists do a realloc. */
	//if ()
	//	void* realloc(void* ptr, uint32_t alloc_size);
	//else
	/* Else do a malloc. */
	//	void* malloc(uint32_t alloc_size);

	/* Now calculate the color of the solid texture. */
	//GL_RGBA32F;


	/*for (y = 0; y < height; y++) {
	for (x = 0; x < width; x = 4) {
	pixels[y + x] = current_bgtexture_data->red;
	pixels[y + x + 1] = current_bgtexture_data->green;
	pixels[y + x + 2] = current_bgtexture_data->blue;
	pixels[y + x + 3] = current_bgtexture_data->opacity;
	}
	}*/
}

static void draw_noise(void *data, void *bgtexture_data)
{
	/*struct bgtexture_data *current_bgtexture_data = bgtexture_data;

	uint16_t x = 0, y = 0;
	uint16_t width = current_bgtexture_data->width;
	uint16_t height = current_bgtexture_data->height;

	float *pixels = data;*/
	//float alloc_size = width * height * 4 * 32;

	/* If the texture already exists do a realloc. */
	//if ()
	//	void* realloc(void* ptr, uint32_t alloc_size);
	//else
	/* Else do a malloc. */
	//	void* malloc(uint32_t alloc_size);

	/* Now calculate the color of the solid texture. */
	//GL_RGBA32F;


	/*for (y = 0; y < height; y++) {
	for (x = 0; x < width; x = 4) {
	pixels[y + x] = current_bgtexture_data->red;
	pixels[y + x + 1] = current_bgtexture_data->green;
	pixels[y + x + 2] = current_bgtexture_data->blue;
	pixels[y + x + 3] = current_bgtexture_data->opacity;
	}
	}*/
}


/* This function reads the updated values in the interface,
   updates the internal settings, then calls the
   fill_texture function to update the texture on the GPU.
   */
static void bgtexture_update(void *data, obs_data_t *settings, void *pixels)
{
	struct bgtexture_data *updated_settings = data;

	uint64_t *updated_pixels = pixels;

	/* Reading updated values from the interface. */
	double red = (double)obs_data_get_double(settings,
		SETTING_RED);
	double green = (double)obs_data_get_double(settings,
		SETTING_GREEN);
	double blue = (double)obs_data_get_double(settings,
		SETTING_BLUE);
	double opacity = (double)obs_data_get_double(settings,
		SETTING_OPACITY);
	uint16_t height = (uint16_t)obs_data_get_int(settings,
		SETTING_HEIGHT);
	uint16_t width = (uint16_t)obs_data_get_int(settings,
		SETTING_WIDTH);

	/* Setting the internal values to match the interface. */
	//updated_settings->red = red;
	//updated_settings->green = green;
	//updated_settings->blue = blue;
	//updated_settings->opacity = opacity;

	//updated_settings->height = height;
	//updated_settings->width = width;

	/* We release the current texture. */
	//free();

	/* We update the texture. */
	fill_texture(pixels, updated_settings);

	/* Upload new texture to the GPU for use. */

}

/* Code to destroy the instance of bgtexture. */
static void bgtexture_destroy(void *data)
{
	struct bgtexture_source *texture = data;

	if (texture) {
		if (texture->initialized) {
			os_event_signal(texture->stop_signal);
			pthread_join(texture->thread, NULL);
		}

		os_event_destroy(texture->stop_signal);
		bfree(texture);
	}
}

static void *video_thread(void *source, void *data)
{
	struct bgtexture_source   *texture_source = source;
	struct bgtexture_data     *texture_data = data;

	/* Now we enable an allocation of memory and a tester. */
	float*                  pixels = NULL;
	float*                  test_realloc = NULL;
	/* Now allocate enough memory for our texture(s) */
	/* Width * Height * 4 channels (RGB+A) * 32bits */
	test_realloc = (float*)realloc(pixels,
		(texture_data->width * texture_data->height
			* 4 * 32));
	if (test_realloc != NULL) {
		pixels = test_realloc;
	}
	else {
		free(pixels);
		//How to send an alert/error message?
		//How to do the equivalent of "exit(1);" ?
	}

	uint64_t            cur_time = os_gettime_ns();

	struct obs_source_frame frame = {
		.data = { [0] = (uint8_t*)pixels },
		.width = texture_data->width,
		.height = texture_data->height,
		.format = VIDEO_FORMAT_RGBA
	};

	while (os_event_try(texture_source->stop_signal) == EAGAIN) {

		fill_texture(pixels, texture_data);
		frame.timestamp = cur_time;
		obs_source_output_video(texture_source->source, &frame);
		os_sleepto_ns(cur_time += 250000000);
	}

	/* It seems a bit weird/a waste of processing power to call
	   a function every time this instance is run. Why not set a
	   flag to check see if the settings need to be updated? I
	   guess it makes sense to call a small function rather then
	   put in an *if* statement. */
	//bgtexture_update(source, settings);

	return NULL;
}

/* This function creates the interface using the built-in OBS types. */
static obs_properties_t *bgtexture_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_int(props, SETTING_WIDTH,
		TEXT_WIDTH, 4, 8192, 4);
	obs_properties_add_int(props, SETTING_HEIGHT,
		TEXT_HEIGHT, 4, 8192, 4);

	obs_properties_add_float_slider(props, SETTING_RED,
		TEXT_RED, 0.0, 1.0, 0.000001);
	obs_properties_add_float_slider(props, SETTING_GREEN,
		TEXT_GREEN, 0.0, 1.0, 0.000001);
	obs_properties_add_float_slider(props, SETTING_BLUE,
		TEXT_BLUE, 0.0, 1.0, 0.000001);
	obs_properties_add_float_slider(props, SETTING_OPACITY,
		TEXT_OPACITY, 0.0, 1.0, 0.000001);
	

	UNUSED_PARAMETER(data);
	return props;
}

/* 
   Sets the defaults for the module so it doesn't load with
   invalid values.
*/

static void bgtexture_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, SETTING_WIDTH, 300);
	obs_data_set_default_int(settings, SETTING_HEIGHT, 300);
	obs_data_set_default_double(settings, SETTING_MASTER_OPACITY, 1.0);
	obs_data_set_default_int(settings, SETTING_POSTFX, 0);
	obs_data_set_default_int(settings, SETTING_LAYER_COUNT, 1);


}

/* 
   Code for OBS to call to create an instance of bgtexture.
   Also calls the destroy functions if the module/instance is
   removed from an OBS scene, or the source added with no
   properties selected (i.e. add->[Source type]->[name it]->
   [Properties window comes up]->[Hit Cancel].
*/
static void *bgtexture_create(obs_data_t *settings, obs_source_t *source)
{
	struct bgtexture_source *bgtexture_instance = bzalloc(sizeof(struct bgtexture_source));
	bgtexture_instance->source = source;

	if (os_event_init(&bgtexture_instance->stop_signal, OS_EVENT_TYPE_MANUAL) != 0) {
		bgtexture_destroy(bgtexture_instance);
		return NULL;
	}

	if (pthread_create(&bgtexture_instance->thread, NULL, video_thread, bgtexture_instance) != 0) {
		bgtexture_destroy(bgtexture_instance);
		return NULL;
	}

	bgtexture_instance->initialized = true;

	obs_source_update(source, settings);

	//UNUSED_PARAMETER(settings);
	//UNUSED_PARAMETER(source);
	return bgtexture_instance;
}



/* This extern enables OBS to grab info about your module. */
extern struct obs_source_info bgtexture;

/*
   This call tells OBS to load your module and register it as a (video)
   source as opposed to a filter, or other such modules.
*/
bool obs_module_load(void)
{
	/*
	This struct provides OBS main() with the base info it needs to
	call functions and methods in your module. Think of it
	as a sort of "Object Method Manager" for C.
	*/
	struct obs_source_info bgtexture = {
		.id = "background_texture",
		.type = OBS_SOURCE_TYPE_INPUT,
		.output_flags = OBS_SOURCE_ASYNC_VIDEO,
		.get_name = bgtexture_getname,
		.create = bgtexture_create,
		.destroy = bgtexture_destroy,
		.update = bgtexture_update,
		.get_properties = bgtexture_properties,
		.get_defaults = bgtexture_defaults
	};

	obs_register_source(&bgtexture);

	return true;
}
