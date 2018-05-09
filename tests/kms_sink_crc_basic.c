/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include "igt.h"
#include "igt_sysfs.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "drm_fourcc.h"


enum color {
	RED,
	GREEN,
};

typedef struct {
	int drm_fd;
	igt_display_t display;
	struct igt_fb fb_green, fb_red;
	igt_plane_t *primary;
} data_t;

static void assert_color(int dir, enum color color)
{
	unsigned int r, g, b;

	igt_require_f(igt_sysfs_scanf(dir,
				      "i915_sink_crc_eDP1",
				      "%4x%4x%4x",
				      &r, &g, &b) == 3,
		      "Sink CRC is unreliable on this machine. Try manual debug with --interactive-debug=no-crc\n");

	/* Black screen is always invalid */
	igt_assert_neq(r | g | b, 0);

	switch (color) {
	case RED:
		igt_assert_lt(0, r);
		igt_assert_eq(0, g);
		igt_assert_eq(0, b);
		igt_debug("sink CRC for red %.4x%.4x%.4x\n", r, g, b);
		break;
	case GREEN:
		igt_assert_eq(0, r);
		igt_assert_lt(0, g);
		igt_assert_eq(0, b);
		igt_debug("sink CRC for green %.4x%.4x%.4x\n", r, g, b);
		break;
	default:
		igt_fail(IGT_EXIT_FAILURE);
	}
}

static void basic_sink_crc_check(data_t *data)
{
	int dir;

	dir = igt_debugfs_dir(data->drm_fd);

	/* Go Green */
	igt_plane_set_fb(data->primary, &data->fb_green);
	igt_display_commit(&data->display);

	/* It should be Green */
	assert_color(dir, GREEN);

	/* Go Red */
	igt_plane_set_fb(data->primary, &data->fb_red);
	igt_display_commit(&data->display);

	/* It should be Red */
	assert_color(dir, RED);

	close(dir);
}

static void run_test(data_t *data)
{
	igt_display_t *display = &data->display;
	igt_output_t *output;
	drmModeModeInfo *mode;
	enum pipe pipe;

	for_each_pipe_with_valid_output(display, pipe, output) {
		drmModeConnectorPtr c = output->config.connector;

		if (c->connector_type != DRM_MODE_CONNECTOR_eDP)
			continue;

		igt_output_set_pipe(output, pipe);

		mode = igt_output_get_mode(output);

		igt_create_color_fb(data->drm_fd,
				    mode->hdisplay, mode->vdisplay,
				    DRM_FORMAT_XRGB8888,
				    LOCAL_I915_FORMAT_MOD_X_TILED,
				    0.0, 1.0, 0.0,
				    &data->fb_green);

		igt_create_color_fb(data->drm_fd,
				    mode->hdisplay, mode->vdisplay,
				    DRM_FORMAT_XRGB8888,
				    LOCAL_I915_FORMAT_MOD_X_TILED,
				    1.0, 0.0, 0.0,
				    &data->fb_red);

		data->primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);

		basic_sink_crc_check(data);
		return;
	}

	igt_skip("no eDP with CRC support found\n");
}

igt_simple_main
{
	data_t data = {};

	igt_skip_on_simulation();

	data.drm_fd = drm_open_driver_master(DRIVER_INTEL);

	kmstest_set_vt_graphics_mode();
	igt_display_init(&data.display, data.drm_fd);

	run_test(&data);

	igt_display_fini(&data.display);
}
