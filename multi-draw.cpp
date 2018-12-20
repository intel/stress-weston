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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <assert.h>
#include <signal.h>
#include <cstdio> // fopen
#include <fstream>

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <sys/time.h>	// gettimeofday
#include <algorithm>	// std::max

#include "multi-draw.h"

#include "shaders.h" // quad_verts/etc
#include "draw-digits.h"

// glm math library
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

// setup code for multi-draw
//------------------------------------------------------------------------------
void initialize_multiDrawArrays(window *window)
{
	GLuint frag, vert;
	GLuint program_multi;	
	GLint status;

	// multi_draw shader 
	vert = create_shader(window, vert_shader_multi, GL_VERTEX_SHADER);
	frag = create_shader(window, frag_shader_short_loop, GL_FRAGMENT_SHADER);

	program_multi = glCreateProgram();
	glAttachShader(program_multi, frag);
	glAttachShader(program_multi, vert);
	glLinkProgram(program_multi);	

	glGetProgramiv(program_multi, GL_LINK_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetProgramInfoLog(program_multi, 1000, &len, log);
		fprintf(stderr, "Error: linking:\n%*s\n", len, log);
		exit(1);
	}
	window->gl_multi.program = program_multi;
	glUseProgram(window->gl_multi.program);

	// get shader attribute location
	window->gl_multi.pos = glGetAttribLocation(window->gl_multi.program, "pos");
	window->gl_multi.col = glGetAttribLocation(window->gl_multi.program, "color");
	window->gl_multi.trans = glGetAttribLocation(window->gl_multi.program, "trans");	
	window->gl_multi.loop_count_short = glGetUniformLocation(window->gl_multi.program, "loop_count");
	//printf("window->gl_multi.loop_count_short: %d\n", window->gl_multi.loop_count_short);

	window->gl_multi.rotation_uniform =
		glGetUniformLocation(window->gl_multi.program, "model_matrix");
	window->gl_multi.view_uniform =
		glGetUniformLocation(window->gl_multi.program, "view");
	window->gl_multi.projection_uniform =
		glGetUniformLocation(window->gl_multi.program, "projection");	

}


// Test scene: seperate draw call for each pyramid
//------------------------------------------------------------------------------
void draw_multiDrawArrays (void* data, struct wl_callback* callback, uint32_t time_now)
{
	struct window *win = (window*)data;
	struct display *display = win->display;

	static const uint32_t speed_div = 5;
	struct wl_region *region;
	EGLint rect[4];
	EGLint buffer_age = 0;

	// callback and weston setup
	assert(win->callback == callback);
	win->callback = NULL;

	if (callback)
		wl_callback_destroy(callback);

	if (display->swap_buffers_with_damage)
		eglQuerySurface(display->egl.dpy, win->egl_surface,
				EGL_BUFFER_AGE_EXT, &buffer_age);

	// timer for moving objects and timing frame
	float fps = calculate_fps(win, "multi_draw", time_now);

	GLfloat angle = (time_now / speed_div) % 360; // * M_PI / 180.0;

	// start GL rendering loop
	glUseProgram(win->gl_multi.program);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// projection matrix	
	glm::mat4 proj_matrix = glm::frustum(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 500.0f);
	glUniformMatrix4fv(win->gl_multi.projection_uniform, 1, GL_FALSE,
		(GLfloat *)glm::value_ptr(proj_matrix));	

	// view matrix
	int maxdim = std::max(x_count, y_count);
	float xCoord = x_count*1.5f - 1.0f;
	float yCoord = y_count*1.5f - 1.0f;

	glm::vec3 v_eye(xCoord, yCoord, -3.0f * (maxdim/2));
	glm::vec3 v_center(xCoord, yCoord, z_count*1.5f);
	glm::vec3 v_up(0.0f, 1.0f, 0.0f);

	glm::mat4 view_matrix = glm::lookAt(v_eye, v_center, v_up);
	glUniformMatrix4fv(win->gl_multi.view_uniform, 1, GL_FALSE,
		(GLfloat *)glm::value_ptr(view_matrix));


	// vertex attribute pointers
	glVertexAttribPointer(win->gl_multi.pos, 3, GL_FLOAT, GL_FALSE, 0, pyramid_verts);
	glVertexAttribPointer(win->gl_multi.col, 4, GL_FLOAT, GL_FALSE, 0, pyramid_colors);	
	glEnableVertexAttribArray(win->gl_multi.pos);
	glEnableVertexAttribArray(win->gl_multi.col);

	// set the number of shader loops
	glUniform1f(win->gl_multi.loop_count_short, win->shortShader_loop_count);

	// draw the grid like mad
	for(int z=(z_count-1); z>=0; z--)
	{
		for(int x=0; x<x_count; x++)
		{
			for(int y=0; y<y_count; y++)			
			{				
				glm::mat4 model_matrix;
				model_matrix = glm::translate(model_matrix, glm::vec3((float)x*3, (float)y*3, (float)z*3));
				model_matrix = glm::rotate(model_matrix, angle*3.14f/180.0f, glm::vec3(0.0f, 1.0f, 0.0f));
				glUniformMatrix4fv(win->gl_multi.rotation_uniform, 1, GL_FALSE,
						   (GLfloat *) glm::value_ptr(model_matrix));				

				glDrawArrays(GL_TRIANGLES, 0, 18);
			}
		}
	}

	glDisableVertexAttribArray(win->gl_multi.pos);
	glDisableVertexAttribArray(win->gl_multi.col);

	// render fps digits
	g_TextRender.DrawDigits(fps, (void*)win, callback, time_now);

	// handle flips/weston
	if (win->opaque || win->fullscreen) {
		region = wl_compositor_create_region(win->display->compositor);
		wl_region_add(region, 0, 0,
			      win->geometry.width,
			      win->geometry.height);
		wl_surface_set_opaque_region(win->surface, region);
		wl_region_destroy(region);
	} else {
		wl_surface_set_opaque_region(win->surface, NULL);
	}

	if(!win->no_swapbuffer_call)
	{
		if (display->swap_buffers_with_damage && buffer_age > 0) {
			rect[0] = win->geometry.width / 4 - 1;
			rect[1] = win->geometry.height / 4 - 1;
			rect[2] = win->geometry.width / 2 + 2;
			rect[3] = win->geometry.height / 2 + 2;
			display->swap_buffers_with_damage(display->egl.dpy,
							  win->egl_surface,
							  rect, 1);
		} else {
			eglSwapBuffers(display->egl.dpy, win->egl_surface);
		}
	}
	win->frames++;
}
