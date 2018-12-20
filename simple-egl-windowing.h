// Copyright 2017 Intel Corporation
// Copyright © 2011 Benjamin Franzke
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// 
// Please see the readme.txt for further license information.

// weston gl specific header includes
#ifndef __SIMPLE_EGL_WINDOWING_H__
#define __SIMPLE_EGL_WINDOWING_H__
#include "config.h"
#include "main.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <linux/input.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include <wayland-cursor.h>

#include <ias-shell-client-protocol.h>

#include "xdg-shell-unstable-v5-client-protocol.h"

#include <sys/types.h>
#include <unistd.h>
#include "protocol/ivi-application-client-protocol.h"
#define IVI_SURFACE_ID 9000

#include <platform.h>


#ifndef EGL_EXT_swap_buffers_with_damage
#define EGL_EXT_swap_buffers_with_damage 1
typedef EGLBoolean (EGLAPIENTRYP PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC)(EGLDisplay dpy, EGLSurface surface, EGLint *rects, EGLint n_rects);
#endif

#ifndef EGL_EXT_buffer_age
#define EGL_EXT_buffer_age 1
#define EGL_BUFFER_AGE_EXT			0x313D
#endif

static int running = 1;

static output* get_default_output(display *display)
{
	struct output *iter;
	int counter = 0;
	wl_list_for_each(iter, &display->output_list, link) {
		if(counter++ == display->window->output)
			return iter;
	}

	// Unreachable, but avoids compiler warning
	return NULL;
}

static void init_egl(display *display, window *window)
{
	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	const char *extensions;

	EGLint config_attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	EGLint major, minor, n, count, i, size;
	EGLConfig *configs;
	EGLBoolean ret;

	if (window->opaque || window->buffer_size == 16)
		config_attribs[10] = 0;

	display->egl.dpy = (EGLDisplay) weston_platform_get_egl_display(EGL_PLATFORM_WAYLAND_KHR,
						display->display, NULL);
	assert(display->egl.dpy);

	ret = eglInitialize(display->egl.dpy, &major, &minor);
	assert(ret == EGL_TRUE);
	ret = eglBindAPI(EGL_OPENGL_ES_API);
	assert(ret == EGL_TRUE);

	if (!eglGetConfigs(display->egl.dpy, NULL, 0, &count) || count < 1)
		assert(0);

	configs = (EGLConfig*) calloc(count, sizeof *configs);
	assert(configs);

	ret = eglChooseConfig(display->egl.dpy, config_attribs,
			      configs, count, &n);
	assert(ret && n >= 1);

	for (i = 0; i < n; i++) {
		eglGetConfigAttrib(display->egl.dpy,
				   configs[i], EGL_BUFFER_SIZE, &size);
		if (window->buffer_size == size) {
			display->egl.conf = configs[i];
			break;
		}
	}
	free(configs);
	if (display->egl.conf == NULL) {
		fprintf(stderr, "did not find config with buffer size %d\n",
			window->buffer_size);
		exit(EXIT_FAILURE);
	}

	display->egl.ctx = eglCreateContext(display->egl.dpy,
					    display->egl.conf,
					    EGL_NO_CONTEXT, context_attribs);
	assert(display->egl.ctx);

	display->swap_buffers_with_damage = NULL;
	extensions = eglQueryString(display->egl.dpy, EGL_EXTENSIONS);
	if (extensions &&
	    strstr(extensions, "EGL_EXT_swap_buffers_with_damage") &&
	    strstr(extensions, "EGL_EXT_buffer_age"))
		display->swap_buffers_with_damage =
			(PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC)
			eglGetProcAddress("eglSwapBuffersWithDamageEXT");

	if (display->swap_buffers_with_damage)
		printf("has EGL_EXT_buffer_age and EGL_EXT_swap_buffers_with_damage\n");

}

static void fini_egl(display *display)
{
	eglTerminate(display->egl.dpy);
	eglReleaseThread();
}



static void handle_surface_configure(void *data, xdg_surface *surface,
			 int32_t width, int32_t height,
			 wl_array *states, uint32_t serial)
{
	struct window *windw = (window*) data;
	void *p;

	windw->fullscreen = 0;
	wl_array_for_each(p, states) {
		uint32_t state = *((uint32_t*) p);
		switch (state) {
		case XDG_SURFACE_STATE_FULLSCREEN:
			windw->fullscreen = 1;
			break;
		}
	}

	if (width > 0 && height > 0) {
		if (!windw->fullscreen) {
			windw->window_size.width = width;
			windw->window_size.height = height;
		}
		windw->geometry.width = width;
		windw->geometry.height = height;
	} else if (!windw->fullscreen) {
		windw->geometry = windw->window_size;
	}

	if (windw->native)
		wl_egl_window_resize(windw->native,
				     windw->geometry.width,
				     windw->geometry.height, 0, 0);

	xdg_surface_ack_configure(surface, serial);
}

static void handle_surface_delete(void *data, xdg_surface *xdg_surface)
{
	running = 0;
}

static const struct xdg_surface_listener xdg_surface_listener = {
	handle_surface_configure,
	handle_surface_delete,
};

static void ias_handle_ping(void *data, ias_surface *ias_surface,
	    uint32_t serial)
{
	ias_surface_pong(ias_surface, serial);
}

static void ias_handle_configure(void *data, ias_surface *ias_surface,
		 int32_t width, int32_t height)
{
	struct window *win;
	win = (window*) data;

	if (win->native) {
		wl_egl_window_resize(win->native, width, height, win->geometry.locx, 
			win->geometry.locy);
	}

	win->geometry.width = width;
	win->geometry.height = height;

	if (!win->fullscreen) {
		win->window_size = win->geometry;
	}
}

static struct ias_surface_listener ias_surface_listener = {
	ias_handle_ping,
	ias_handle_configure,
};

static void handle_ivi_surface_configure(void *data, ivi_surface *ivi_surface,
                             int32_t width, int32_t height)
{
	struct window *win;
	win = (window*)data;

	wl_egl_window_resize(win->native, width, height, 0, 0);

	win->geometry.width = width;
	win->geometry.height = height;

	if (!win->fullscreen)
		win->window_size = win->geometry;
}

static const struct ivi_surface_listener ivi_surface_listener = {
	handle_ivi_surface_configure,
};

static void create_xdg_surface(window *window, display *display)
{
	window->xdg_surface = xdg_shell_get_xdg_surface(display->shell,
							window->surface);

	xdg_surface_add_listener(window->xdg_surface,
				 &xdg_surface_listener, window);

	xdg_surface_set_title(window->xdg_surface, "stress-weston");

	if (window->fullscreen)
	{
		xdg_surface_set_fullscreen(window->xdg_surface, NULL);
		printf("create_xdg_surface fullscreen = true\n");
	}
}

static void create_ivi_surface(window *window, display *display)
{
	uint32_t id_ivisurf = IVI_SURFACE_ID + (uint32_t)getpid();
	window->ivi_surface =
		ivi_application_surface_create(display->ivi_application,
					       id_ivisurf, window->surface);

	if (window->ivi_surface == NULL) {
		fprintf(stderr, "Failed to create ivi_client_surface\n");
		abort();
	}

	ivi_surface_add_listener(window->ivi_surface,
				 &ivi_surface_listener, window);
}

static void create_ias_surface(window *window, display *display)
{
	window->shell_surface = ias_shell_get_ias_surface(display->ias_shell,
			window->surface, "stress-weston");
	ias_surface_add_listener(window->shell_surface,
			&ias_surface_listener, window);

	if (window->fullscreen) {
		output* defaultOutput = get_default_output(display);
		if(defaultOutput) {
			ias_surface_set_fullscreen(window->shell_surface, 
										defaultOutput->output);
		} else {
			fprintf(stderr, "Failed to get default output when setting fullscreen mode.\n");
		}
	} else {
		ias_surface_unset_fullscreen(display->window->shell_surface, 
			display->window->geometry.width, display->window->geometry.height);
		ias_shell_set_zorder(display->ias_shell, window->shell_surface, 0);
	}
}

static void create_surface(window *window)
{
	struct display *display = window->display;
	EGLBoolean ret;

	window->surface = wl_compositor_create_surface(display->compositor);

	window->native =
		wl_egl_window_create(window->surface,
				     window->geometry.width,
				     window->geometry.height);
	window->egl_surface =
		weston_platform_create_egl_surface(display->egl.dpy,
						   display->egl.conf,
						   window->native, NULL);

	if(!window->no_swapbuffer_call)
	{
		if (display->shell) {
			printf("Created XDG SURFACE\n");
			create_xdg_surface(window, display);
		} else if (display->ivi_application ) {
			printf("Created IVI SURFACE\n");
			create_ivi_surface(window, display);
		} else if (display->ias_shell) {
			printf("Created IAS SURFACE\n");
			create_ias_surface(window, display);
		}
		else {
			assert(0);
		}
	}

	ret = eglMakeCurrent(window->display->egl.dpy, window->egl_surface,
			     window->egl_surface, window->display->egl.ctx);
	assert(ret == EGL_TRUE);

	if (!window->frame_sync)
		eglSwapInterval(display->egl.dpy, 0);
}

static void destroy_surface(window *window)
{
	/* Required, otherwise segfault in egl_dri2.c: dri2_make_current()
	 * on eglReleaseThread(). */
	eglMakeCurrent(window->display->egl.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
		       EGL_NO_CONTEXT);

	eglDestroySurface(window->display->egl.dpy, window->egl_surface);
	wl_egl_window_destroy(window->native);

	if (window->xdg_surface)
		xdg_surface_destroy(window->xdg_surface);
	if (window->display->ivi_application)
		ivi_surface_destroy(window->ivi_surface);
	if (window->display->ias_shell) {
		ias_surface_destroy(window->shell_surface);
	}
	wl_surface_destroy(window->surface);

	if (window->callback)
		wl_callback_destroy(window->callback);
}


static void pointer_handle_enter(void *data, wl_pointer *pointer,
		     uint32_t serial, wl_surface *surface,
		     wl_fixed_t sx, wl_fixed_t sy)
{
	struct display *disp = (display*)data;	
	struct wl_cursor *cursor = disp->default_cursor; // we don't need to use the cursor
	struct wl_cursor_image *image;


	if (disp->window->fullscreen)
		wl_pointer_set_cursor(pointer, serial, NULL, 0, 0);
	else if (cursor) {
		image = disp->default_cursor->images[0];
#ifdef WL_CURSOR_LIB_ERROR		
		buffer = wl_cursor_image_get_buffer(image);
		if (!buffer)
			return;
		wl_pointer_set_cursor(pointer, serial,
				      disp->cursor_surface,
				      image->hotspot_x,
				      image->hotspot_y);
		wl_surface_attach(disp->cursor_surface, buffer, 0, 0);
		wl_surface_damage(disp->cursor_surface, 0, 0,
				  image->width, image->height);
		wl_surface_commit(disp->cursor_surface);
#endif

	}
}

static void pointer_handle_leave(void *data, wl_pointer *pointer,
		     uint32_t serial, wl_surface *surface)
{
}

static void pointer_handle_motion(void *data, wl_pointer *pointer,
		      uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
}

static void pointer_handle_button(void *data, wl_pointer *wl_pointer,
		      uint32_t serial, uint32_t time, uint32_t button,
		      uint32_t state)
{
	struct display *disp = (display*)data;

	if (!disp->window->xdg_surface)
		return;

	if (button == BTN_LEFT && state == WL_POINTER_BUTTON_STATE_PRESSED
			&& disp->shell)
		xdg_surface_move(disp->window->xdg_surface,
						 disp->seat, serial);
}

static void pointer_handle_axis(void *data, wl_pointer *wl_pointer,
		    uint32_t time, uint32_t axis, wl_fixed_t value)
{
}

static const struct wl_pointer_listener pointer_listener = {
	pointer_handle_enter,
	pointer_handle_leave,
	pointer_handle_motion,
	pointer_handle_button,
	pointer_handle_axis,
};

static void touch_handle_down(void *data, wl_touch *wl_touch,
		  uint32_t serial, uint32_t time, wl_surface *surface,
		  int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{
	struct display *d = (display *)data;

	if(d->shell) {
		xdg_surface_move(d->window->xdg_surface, d->seat, serial);
	}
}

static void touch_handle_up(void *data, wl_touch *wl_touch,
		uint32_t serial, uint32_t time, int32_t id)
{
}

static void touch_handle_motion(void *data, wl_touch *wl_touch,
		    uint32_t time, int32_t id, wl_fixed_t x_w, wl_fixed_t y_w)
{

}

static void touch_handle_frame(void *data, wl_touch *wl_touch)
{
}

static void touch_handle_cancel(void *data, wl_touch *wl_touch)
{
}

static const struct wl_touch_listener touch_listener = {
	touch_handle_down,
	touch_handle_up,
	touch_handle_motion,
	touch_handle_frame,
	touch_handle_cancel,
};

static void keyboard_handle_keymap(void *data, wl_keyboard *keyboard,
		       uint32_t format, int fd, uint32_t size)
{
}

static void keyboard_handle_enter(void *data, wl_keyboard *keyboard,
		      uint32_t serial, wl_surface *surface,
		      wl_array *keys)
{
}

static void keyboard_handle_leave(void *data, wl_keyboard *keyboard,
		      uint32_t serial, wl_surface *surface)
{
}

static void keyboard_handle_key(void *data, wl_keyboard *keyboard,
		    uint32_t serial, uint32_t time, uint32_t key,
		    uint32_t state)
{
	struct display *d = (display*)data;

	static int count = 0;

	count++;
	if (count%2!=1)	
		return; // we always get double-presses on keys

	// print the keycode for testing
	//printf("key: %d state: %d\n", key, state);

	if (key == KEY_F11 && state) {
		if (d->shell) {
			if (d->window->fullscreen) {
				xdg_surface_unset_fullscreen(d->window->xdg_surface);				
			}
			else
				xdg_surface_set_fullscreen(d->window->xdg_surface, NULL);
		}
	} 
	else if (key == KEY_ESC && state) 
	{
		running = 0;
	} 
	else if (key == 46) //'c'	
	{ 
		swap_draw_case(d->window);	
	} 
	else if (key == 16) // 'q'
	{
		add_shader_loops(multiDrawArrays);
	} 
	else if(key == 30) // 'a'
	{		
		shrink_shader_loops(multiDrawArrays);
	}
	else if ((key == 78) || (key == 13)) //'+'
	{ 
		switch(g_draw_case)
		{
			case simpleDial:
				add_shader_loops(simpleDial);			
				break;

			case multiDrawArrays:
			case singleDrawArrays:
			case batchDrawArrays:
				add_pyramids();				
				break;

			case longShader:
				add_shader_loops(longShader);				
				break;

			default:
				break;

		}
		
	} 
	else if ((key == 74) || (key == 12)) //'-'
	{ 
		switch(g_draw_case)
		{
			case simpleDial:
				shrink_shader_loops(simpleDial);			
				break;

			case multiDrawArrays:
			case singleDrawArrays:
			case batchDrawArrays:
				remove_pyramids();
				break;

			case longShader:
				shrink_shader_loops(longShader);
				break;
			
			default:
				break;

		}		

	} else if (d->ias_shell) {
		if (d->window->fullscreen) {
			ias_surface_set_fullscreen(d->window->shell_surface,
					get_default_output(d)->output);
		} else {
			;
			ias_surface_unset_fullscreen(d->window->shell_surface, 
				d->window->geometry.width, d->window->geometry.height);
			ias_shell_set_zorder(d->ias_shell,
					d->window->shell_surface, 0);
		}	
	}
}

static void keyboard_handle_modifiers(void *data, wl_keyboard *keyboard,
			  uint32_t serial, uint32_t mods_depressed,
			  uint32_t mods_latched, uint32_t mods_locked,
			  uint32_t group)
{
}

static const struct wl_keyboard_listener keyboard_listener = {
	keyboard_handle_keymap,
	keyboard_handle_enter,
	keyboard_handle_leave,
	keyboard_handle_key,
	keyboard_handle_modifiers,
};

static void seat_handle_capabilities(void *data, wl_seat *seat,
			unsigned int caps)
			 //enum wl_seat_capability caps)
{
	struct display *d = (display*)data;

	if ((caps & WL_SEAT_CAPABILITY_POINTER) && !d->pointer) {
		d->pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(d->pointer, &pointer_listener, d);
	} else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && d->pointer) {
		wl_pointer_destroy(d->pointer);
		d->pointer = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !d->keyboard) {
		d->keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(d->keyboard, &keyboard_listener, d);
	} else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && d->keyboard) {
		wl_keyboard_destroy(d->keyboard);
		d->keyboard = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !d->touch) {
		d->touch = wl_seat_get_touch(seat);
		wl_touch_set_user_data(d->touch, d);
		wl_touch_add_listener(d->touch, &touch_listener, d);
	} else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && d->touch) {
		wl_touch_destroy(d->touch);
		d->touch = NULL;
	}
}

static const struct wl_seat_listener seat_listener = {
	seat_handle_capabilities,
};

static void xdg_shell_ping(void *data, xdg_shell *shell, uint32_t serial)
{
	xdg_shell_pong(shell, serial);
}

static const struct xdg_shell_listener xdg_shell_listener = {
	xdg_shell_ping,
};

#define XDG_VERSION 5 /* The version of xdg-shell that we implement */
#ifdef static_assert
static_assert(XDG_VERSION == XDG_SHELL_VERSION_CURRENT,
	      "Interface version doesn't match implementation version");
#endif

static void display_add_output(display *d, uint32_t id)
{
	struct output *out;

	out = (output*)malloc(sizeof *out);
	if (out == NULL)
		return;

	memset(out, 0, sizeof *out);
	out->display = d;
	out->output = (wl_output*)
		wl_registry_bind(d->registry, id, &wl_output_interface, 1);
	wl_list_insert(d->output_list.prev, &out->link);
}

static void registry_handle_global(void *data, wl_registry *registry,
		       uint32_t name, const char *interface, uint32_t version)
{
	struct display *d = (display*) data;

	if (strcmp(interface, "wl_compositor") == 0) {
		d->compositor = (wl_compositor*)
			wl_registry_bind(registry, name,
					 &wl_compositor_interface, 1);
	} else if (strcmp(interface, "xdg_shell") == 0) {
		if (!d->ias_shell) {
			d->shell = (xdg_shell*) wl_registry_bind(registry, name,
							 &xdg_shell_interface, 1);
			xdg_shell_add_listener(d->shell, &xdg_shell_listener, d);
			xdg_shell_use_unstable_version(d->shell, XDG_VERSION);
		}
	} else if (strcmp(interface, "ias_shell") == 0) {
		if (!d->shell) {
			d->ias_shell = (ias_shell*) wl_registry_bind(registry, name,
					 &ias_shell_interface, 1);
		}
	} else if (strcmp(interface, "wl_seat") == 0) {
		d->seat = (wl_seat*)wl_registry_bind(registry, name,
					    &wl_seat_interface, 1);
		wl_seat_add_listener(d->seat, &seat_listener, d);
	} else if (strcmp(interface, "wl_output") == 0) {
		display_add_output(d, name);
	} else if (strcmp(interface, "wl_shm") == 0) {
		d->shm = (wl_shm*)wl_registry_bind(registry, name,
					  &wl_shm_interface, 1);
#ifdef WL_CURSOR_LIB_ERROR		
		d->cursor_theme = wl_cursor_theme_load(NULL, 32, d->shm);
		if (!d->cursor_theme) {
			fprintf(stderr, "unable to load default theme\n");
			return;
		}
		d->default_cursor =
			wl_cursor_theme_get_cursor(d->cursor_theme, "left_ptr");
		if (!d->default_cursor) {
			fprintf(stderr, "unable to load default left pointer\n");
			// TODO: abort ?
		}
#endif		
	} else if (strcmp(interface, "ivi_application") == 0) {
		d->ivi_application = (ivi_application*)
			wl_registry_bind(registry, name,
					 &ivi_application_interface, 1);
	}
}

static void registry_handle_global_remove(void *data, wl_registry *registry,
			      uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

static void signal_int(int signum)
{
	running = 0;
}
#endif // __SIMPLE_EGL_WINDOWING_H__
