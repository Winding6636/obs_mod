/******************************************************************************
Based on code Copyright (c) 2015 John R. Bradley <jrb@turrettech.com>
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

#include <libavfilter/avfilter.h>

/*
 * LIBAVFILTER_VERSION_CHECK checks the version of libavfilter and FFmpeg.
 * a is the major version (shared by ffmpeg and libavfilter).
 * b and c the minor and micro versions of libav.
 * d and e the minor and micro versions of FFmpeg.
 */
#define LIBAVFILTER_VERSION_CHECK( a, b, c, d, e ) \
    ( (LIBAVFILTER_VERSION_MICRO <  100 && LIBAVFILTER_VERSION_INT >= AV_VERSION_INT( a, b, c ) ) || \
      (LIBAVFILTER_VERSION_MICRO >= 100 && LIBAVFILTER_VERSION_INT >= AV_VERSION_INT( a, d, e ) ) )

#if !LIBAVFILTER_VERSION_CHECK(54, 28, 0, 59, 100)
	#define avcodec_free_frame av_freep
#endif

#if LIBAVFILTER_VERSION_INT < 0x371c01
	#define av_frame_alloc avcodec_alloc_frame
	#define av_frame_unref avcodec_get_frame_defaults
	#define av_frame_free avcodec_free_frame
#endif

#if LIBAVFILTER_VERSION_MAJOR >= 57
	#define av_free_packet av_packet_unref
#endif
