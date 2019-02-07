// Copyright 2017 Intel Corporation
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in 
// the Software without restriction, including without limitation the rights to 
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
// of the Software, and to permit persons to whom the Software is furnished to do
// so, subject to the following conditions:
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
#ifndef __MAIN_H__
#define __MAIN_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <signal.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include <wayland-cursor.h>

// defines
#define WESTON_EGL
#define WINDOW_WIDTH 	1920 
#define WINDOW_HEIGHT 	1080 
#define FRAME_TIME_RECORDING_WINDOW_SIZE 500


// structs/forward declaretions
enum DrawCases {
	simpleDial = 0,	
	singleDrawArrays=1,
	multiDrawArrays=2,
	simpleTexture=3,
	longShader=4,
	batchDrawArrays=5,	
	multiTexture=6,	// future case
	largeTexture=7, // future case
	next_case,
};

// EGL priority hint
enum eContextPriority {
	High = 0,	
	Medium = 1,
	Low = 2,
};


// Opengl and surface/window structs
struct output {
	struct display *display;
	struct wl_output *output;
	struct wl_list link;
};

struct display {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct ias_shell *ias_shell;
	struct xdg_shell *shell;
	struct wl_seat *seat;
	struct wl_pointer *pointer;
	struct wl_touch *touch;
	struct wl_keyboard *keyboard;
	struct wl_shm *shm;
	struct wl_cursor_theme *cursor_theme;
	struct wl_cursor *default_cursor;
	struct wl_surface *cursor_surface;
	struct {
		EGLDisplay dpy;
		EGLContext ctx;
		EGLConfig conf;
	} egl;
	struct window *window;
	struct ivi_application *ivi_application;
	struct wl_list output_list;

	PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC swap_buffers_with_damage;
};

struct geometry {
	int width, height;
	int locx, locy;
};

struct window {	
	struct display *display;
	struct geometry geometry, window_size;

	// the different scene parameters
	// single draw scene
	struct {
		GLuint program;
		GLuint rotation_uniform;
		GLuint projection_uniform;
		GLuint view_uniform;
		GLuint pos;
		GLuint col;
		GLuint trans;
		GLuint loop_count_short;
	} gl_single;

	// multi draw scene
	struct {
		GLuint program;
		GLuint rotation_uniform;
		GLuint projection_uniform;
		GLuint view_uniform;
		GLuint pos;
		GLuint col;
		GLuint trans;
		GLuint loop_count_short;
	} gl_multi;

	// default texture scene
	struct {
		GLuint program;
		GLuint rotation_uniform;
		GLuint projection_uniform;
		GLuint view_uniform;
		GLuint pos;
		GLuint col;
		GLuint trans;
		GLuint tex1;
		GLuint shader_loop_count;
	} gl_tex;

	// texture/blur scene
	struct {
		GLuint program;
		GLuint rotation_uniform;
		GLuint projection_uniform;
		GLuint view_uniform;
		GLuint pos;
		GLuint col;
		GLuint trans;
		GLuint tex1;
		GLuint shader_loop_count;
		GLuint blur_radius;
	} gl_tex_blur;

	// texture flat grey shader scene
	struct {
		GLuint program;
		GLuint rotation_uniform;
		GLuint projection_uniform;
		GLuint view_uniform;
		GLuint pos;
		GLuint col;
		GLuint trans;
		GLuint tex1;
		GLuint shader_loop_count;
	} gl_flat;

	// long shader scene
	struct {
		GLuint program;
		GLuint rotation_uniform;
		GLuint projection_uniform;
		GLuint view_uniform;
		GLuint loopcount_uniform;
		GLuint pos;
		GLuint col;
		GLuint trans;
		GLuint tex1;
	} gl_longShader;

	// weston/gl structures
	struct wl_egl_window *native;
	struct wl_surface *surface;
	struct ias_surface *shell_surface;
	struct xdg_surface *xdg_surface;
	struct ivi_surface *ivi_surface;
	EGLSurface egl_surface;
	struct wl_callback *callback;
	int fullscreen, opaque, buffer_size, frame_sync, output;
	
	// global scene parameters
	uint32_t benchmark_time, frames;	
	int offscreen;
	bool no_swapbuffer_call;
	bool texture_flat_no_rotate;
	float texture_fetch_radius;
	float longShader_loop_count;
	float shortShader_loop_count;
	float dialsShader_loop_count;
};

// forward declarations
class textRender;

// globals
extern DrawCases g_draw_case;
extern eContextPriority g_contextPriority;
extern bool g_demo_mode;

// pyramid geometry parameters
extern int x_count;
extern int y_count;
extern int z_count;
extern int g_batchSize;
extern textRender g_TextRender;
extern bool g_recordMetrics;


//digits
extern GLuint g_digitTextureID;

//dials
extern GLuint g_dialTexID;
extern GLuint g_needleTexID;

//pyramids
extern GLfloat* pyramid_positions; 
extern GLfloat* pyramid_colors_single_draw; 
extern GLfloat* pyramid_transforms; 

// texture
extern GLuint g_textureID;

// functions
void add_shader_loops(DrawCases scene);
void shrink_shader_loops(DrawCases scene);
void add_pyramids();
void remove_pyramids();
void swap_draw_case(window* win, DrawCases drawcase = next_case);

// helpers
float calculate_fps(window *win, char* test_name, uint32_t& time_now);

#endif // __MAIN_H__
