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
#include <signal.h>
#include <cstdio> // fopen
#include <fstream>

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <sys/time.h>	// gettimeofday
#include <algorithm>	// std::max

#include "single-draw.h"

#include "shaders.h" // quad_verts/etc
#include "draw-digits.h"

// glm math library
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

// setup code for batch draw
//------------------------------------------------------------------------------
void initialize_batchDrawArrays(window *window)
{
	GLuint frag, vert;
	GLint status;

	// batch_draw shader
	if(0 == window->gl_single.program) {
		vert = create_shader(window, vert_shader_single, GL_VERTEX_SHADER);	
		frag = create_shader(window, frag_shader_short_loop, GL_FRAGMENT_SHADER);

		window->gl_single.program = glCreateProgram();
		glAttachShader(window->gl_single.program, frag);
		glAttachShader(window->gl_single.program, vert);
		glLinkProgram(window->gl_single.program);

		glGetProgramiv(window->gl_single.program, GL_LINK_STATUS, &status);
		if (!status) {
			char log[1000];
			GLsizei len;
			glGetProgramInfoLog(window->gl_single.program, 1000, &len, log);
			fprintf(stderr, "Error: linking:\n%*s\n", len, log);
			exit(1);
		}
	}
	glUseProgram(window->gl_single.program);

	// get shader attribute location
	window->gl_single.pos = glGetAttribLocation(window->gl_single.program, "pos");
	window->gl_single.col = glGetAttribLocation(window->gl_single.program, "color");	
	window->gl_single.trans = glGetAttribLocation(window->gl_single.program, "trans");
	
	window->gl_single.rotation_uniform =
		glGetUniformLocation(window->gl_single.program, "model_matrix");
	window->gl_single.view_uniform =
		glGetUniformLocation(window->gl_single.program, "view");
	window->gl_single.projection_uniform =
		glGetUniformLocation(window->gl_single.program, "projection");
	window->gl_single.loop_count_short = 
		glGetUniformLocation(window->gl_single.program, "loop_count");
}


// Test scene: draw full screen of pyramids with single draw call
//------------------------------------------------------------------------------
void draw_batchDrawArrays(void *data, struct wl_callback *callback, uint32_t time_now)
{
	struct window *win = (window*)data;
	struct display *display = win->display;

	static const uint32_t speed_div = 5;
	struct wl_region *region;
	EGLint rect[4];
	EGLint buffer_age = 0;

	// callback and weston management
	assert(win->callback == callback);
	win->callback = NULL;

	if (callback)
		wl_callback_destroy(callback);

	if (display->swap_buffers_with_damage)
		eglQuerySurface(display->egl.dpy, win->egl_surface,
				EGL_BUFFER_AGE_EXT, &buffer_age);

	// timer for moving objects and timing frame
	float fps = calculate_fps(win, "batch_draw", time_now);

	GLfloat angle = (time_now / speed_div) % 360; // * M_PI / 180.0;
	

	// start the GL loop
	glUseProgram(win->gl_single.program);	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// projection matrix	
	glm::mat4 proj_matrix = glm::frustum(-1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 500.0f);
	glUniformMatrix4fv(win->gl_single.projection_uniform, 1, GL_FALSE,
		(GLfloat *)glm::value_ptr(proj_matrix));

	// view matrix - make frustum so it bounds the pyramid grid tightly
	int maxdim = std::max(x_count, y_count);
	float xCoord = x_count*1.5f - 1.0f;
	float yCoord = y_count*1.5f - 1.0f;

	glm::vec3 v_eye(xCoord, yCoord, -3.0f * (maxdim/2));
	glm::vec3 v_center(xCoord, yCoord, z_count*1.5f);
	glm::vec3 v_up(0.0f, 1.0f, 0.0f);

	glm::mat4 view_matrix = glm::lookAt(v_eye, v_center, v_up);
	glUniformMatrix4fv(win->gl_single.view_uniform, 1, GL_FALSE,
		(GLfloat *)glm::value_ptr(view_matrix));

	// set the vertex buffers
	glVertexAttribPointer(win->gl_single.pos, 3, GL_FLOAT, GL_FALSE, 0, pyramid_positions); 
	glVertexAttribPointer(win->gl_single.col, 4, GL_FLOAT, GL_FALSE, 0, pyramid_colors_single_draw);
	glVertexAttribPointer(win->gl_single.trans, 3,GL_FLOAT,GL_FALSE, 0, pyramid_transforms);

	glEnableVertexAttribArray(win->gl_single.pos);
	glEnableVertexAttribArray(win->gl_single.col);
	glEnableVertexAttribArray(win->gl_single.trans);

	// set the number of shader 'work' loops
	glUniform1f(win->gl_single.loop_count_short, win->shortShader_loop_count);

	// set the pyramid rotation matrix
	glm::mat4 identity_matrix(1.f);
	glm::mat4 model_matrix = glm::rotate(identity_matrix, angle*(3.14f/180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	glUniformMatrix4fv(win->gl_single.rotation_uniform, 1, GL_FALSE,
			   (GLfloat *) glm::value_ptr(model_matrix));

	// check validity of batching
	if(g_batchSize > (x_count * y_count * z_count))
	{
		g_batchSize = x_count * y_count * z_count;
	}

	if(g_batchSize < 1)
	{
		g_batchSize = 1;
	}

	// draw
	float block_size = x_count * y_count * z_count;	
	block_size /= g_batchSize;
	if(block_size < 1.0f) {
		block_size = 1.0f;
	}
	GLuint ublock_size = (GLuint)block_size;	
	//printf("block size, # pyramids per group: %d\n", ublock_size);
	//printf("g_batchSize: %d\n", g_batchSize);
	//printf("\n");
	GLuint draw_size = (18 * ublock_size);
	GLuint start_index = 0;
	
	start_index = 0;
	GLint total_count = 0;
	for(int i=0; i<g_batchSize; i++)
	{
		//glDrawArrays(GL_TRIANGLES, 0, 18 * (x_count * y_count * z_count));
		//printf("%d: %d - %d, ", i, start_index, start_index+18*ublock_size);

		glDrawArrays(GL_TRIANGLES, start_index, draw_size);

		start_index+=draw_size;
		total_count+=ublock_size;
	}

	// handle the odd-sized final batch (if there is one)
	if(total_count < (x_count * y_count * z_count)) {		
		GLuint final_block_size = 18*((x_count * y_count * z_count)-(ublock_size*g_batchSize));
		//printf(" final block: %d - %d \n", start_index, start_index+final_block_size);
		glDrawArrays(GL_TRIANGLES, start_index, final_block_size);
	}


	glDisableVertexAttribArray(win->gl_single.pos);
	glDisableVertexAttribArray(win->gl_single.col);
	glDisableVertexAttribArray(win->gl_single.trans);

	// render the FPS 
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
