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
#include <cstdio> 		// fopen
#include <fstream>

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <sys/time.h>	// gettimeofday

#include "long-shader.h"

#include "shaders.h" 	// quad_verts/etc
#include "draw-digits.h"

// textures
#include "dialface.h"

// glm math library
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

// setup code for long shader
//------------------------------------------------------------------------------
void initialize_longShader(window *window)
{
	GLuint frag, vert;
	GLint status;

	// long shader
	vert = create_shader(window, vert_shader_textured, GL_VERTEX_SHADER);
	frag = create_shader(window, frag_shader_long_shader_textured, GL_FRAGMENT_SHADER);
	
	window->gl_longShader.program = glCreateProgram();
	glAttachShader(window->gl_longShader.program, frag);
	glAttachShader(window->gl_longShader.program, vert);
	glLinkProgram(window->gl_longShader.program);

	glGetProgramiv(window->gl_longShader.program, GL_LINK_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetProgramInfoLog(window->gl_longShader.program, 1000, &len, log);
		fprintf(stderr, "Error: linking:\n%*s\n", len, log);
		exit(1);
	}

	glUseProgram(window->gl_longShader.program);

	// get shader attribute bind locations
	window->gl_longShader.pos = glGetAttribLocation(window->gl_tex.program, "pos");
	window->gl_longShader.col = glGetAttribLocation(window->gl_tex.program, "color");
	window->gl_longShader.trans = glGetAttribLocation(window->gl_tex.program, "trans");
	window->gl_longShader.tex1 = glGetAttribLocation(window->gl_tex.program, "texcoord1");

	window->gl_longShader.rotation_uniform = glGetUniformLocation(window->gl_longShader.program, "model_matrix");
	window->gl_longShader.view_uniform = glGetUniformLocation(window->gl_longShader.program, "view");
	window->gl_longShader.projection_uniform = glGetUniformLocation(window->gl_longShader.program, "projection");
	window->gl_longShader.loopcount_uniform = glGetUniformLocation(window->gl_longShader.program, "loop_count");

	// load textures
	if(0==g_dialTexID) 
	{
		registerTexture(g_dialTexID, image_dial::width, image_dial::height, image_dial::header_data, 2);
	}	
}

// Test scene: fullscreen quad with variable compute shader
//-----------------------------------------------------------------------------
void draw_longShader(void *data, struct wl_callback *callback, uint32_t time_now)
{
	struct window *win = (window*)data;
	struct display *display = win->display;
	static const uint32_t speed_div = 5;
	EGLint buffer_age = 0;
	
	struct wl_region *region;
	EGLint rect[4];

	// callback and weston setup
	assert(win->callback == callback);
	win->callback = NULL;

	if (callback)
		wl_callback_destroy(callback);

	// timer for moving objects and timing frame
	float fps = calculate_fps(win, "long_shader", time_now);

	GLfloat angle = (time_now / (speed_div * 2)) % 360; 
	
	glViewport(0, 0, win->geometry.width, win->geometry.height);
	glUseProgram(win->gl_longShader.program);	

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_dialTexID);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// orthographic projection matrix	
	float r=1.0f;
	float l=-1.0f;
	float t=1.0f;
	float b=-1.0f;
	float n=-1.0f;
	float f=1.0f;

	glm::mat4 ortho_matrix = glm::ortho(l,r,b,t,n,f);
	glUniformMatrix4fv(win->gl_tex.projection_uniform, 1, GL_FALSE,
			   (GLfloat *) glm::value_ptr(ortho_matrix));

	// view matrix
	glm::vec3 v_eye(0.0f, 0.0f, -1.0f);
	glm::vec3 v_center(0.0f, 0.0f, 0.0f);
	glm::vec3 v_up(0.0f, 1.0f, 0.0f);

	glm::mat4 view_matrix = glm::lookAt(v_eye, v_center, v_up);
	glUniformMatrix4fv(win->gl_tex.view_uniform, 1, GL_FALSE,
		(GLfloat *)glm::value_ptr(view_matrix));

	// set vertex arrays
	glVertexAttribPointer(win->gl_longShader.pos, 3, GL_FLOAT, GL_FALSE, 0, quad_verts);
	glVertexAttribPointer(win->gl_longShader.col, 4, GL_FLOAT, GL_FALSE, 0, quad_colors);
	glVertexAttribPointer(win->gl_longShader.tex1, 2, GL_FLOAT, GL_FALSE, 0, quad_texcoords);
	glEnableVertexAttribArray(win->gl_longShader.pos);
	glEnableVertexAttribArray(win->gl_longShader.col);
	glEnableVertexAttribArray(win->gl_longShader.tex1);

	// fullscreen quad
	glm::mat4 identity_matrix(1.f);
	glm::mat4 model_matrix = glm::rotate(identity_matrix, angle*(3.14159265f/180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	glUniformMatrix4fv(win->gl_longShader.rotation_uniform, 1, GL_FALSE, (GLfloat *)glm::value_ptr(model_matrix));

	// set work amount
	glUniform1f(win->gl_longShader.loopcount_uniform, win->longShader_loop_count);

	// draw fullscreen quad
	glDrawArrays(GL_TRIANGLES, 0, 6);

	// draw fps
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
