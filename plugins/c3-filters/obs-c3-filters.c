#include <obs-module.h>
#include "obs-c3-filters-config.h"

OBS_DECLARE_MODULE()

OBS_MODULE_USE_DEFAULT_LOCALE("obs-filters", "en-US")

extern struct obs_source_info async_delay_filter;
extern struct obs_source_info chroma_key_filter;
extern struct obs_source_info color_filter;
extern struct obs_source_info color_grade_filter;
extern struct obs_source_info color_key_filter;
extern struct obs_source_info compressor_filter;
extern struct obs_source_info crop_filter;
extern struct obs_source_info crosshatching_filter;
extern struct obs_source_info cross_stitching_filter;
extern struct obs_source_info compressor_filter;
extern struct obs_source_info droste_filter;
extern struct obs_source_info gain_filter;
extern struct obs_source_info gaussian_blur_filter;
extern struct obs_source_info gpu_delay_filter;
extern struct obs_source_info like_a_dream_filter;
extern struct obs_source_info luma_key_filter;
extern struct obs_source_info mask_filter;
extern struct obs_source_info night_vision_filter;
extern struct obs_source_info noise_gate_filter;

#if SPEEXDSP_ENABLED
	extern struct obs_source_info noise_suppress_filter;
#endif

extern struct obs_source_info pixelation_filter;
extern struct obs_source_info posterize_filter;
extern struct obs_source_info radial_blur_filter;
extern struct obs_source_info scale_filter;
extern struct obs_source_info scroll_filter;
extern struct obs_source_info sepia_filter;
extern struct obs_source_info sharpness_filter;

bool obs_module_load(void)
{
	obs_register_source(&async_delay_filter);
	obs_register_source(&chroma_key_filter);
	obs_register_source(&color_filter);
	obs_register_source(&color_key_filter);
	obs_register_source(&color_grade_filter);
	obs_register_source(&compressor_filter);
	obs_register_source(&crop_filter);
	obs_register_source(&crosshatching_filter);
	obs_register_source(&cross_stitching_filter);
	obs_register_source(&droste_filter);
	obs_register_source(&gain_filter);
	obs_register_source(&gaussian_blur_filter);
	obs_register_source(&gpu_delay_filter);
	obs_register_source(&like_a_dream_filter);
	obs_register_source(&luma_key_filter);
	obs_register_source(&mask_filter);
	obs_register_source(&night_vision_filter);
	obs_register_source(&noise_gate_filter);

	#if SPEEXDSP_ENABLED
		obs_register_source(&noise_suppress_filter);
	#endif

	obs_register_source(&pixelation_filter);
	obs_register_source(&posterize_filter);
	obs_register_source(&radial_blur_filter);
	obs_register_source(&scale_filter);
	obs_register_source(&scroll_filter);
	obs_register_source(&sepia_filter);
	obs_register_source(&sharpness_filter);

	return true;
}
