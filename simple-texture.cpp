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

#include "simple-texture.h"

#include "shaders.h" // quad_verts/etc
#include "draw-digits.h"

// textures
#include "store1k.h"

// glm math library
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

// setup code for spinning large texture
//------------------------------------------------------------------------------
void initialize_simpleTexture(window *window)
{
	GLuint frag, vert;
	GLint status;
	
	// also loaded by dials
	// A simple texture sampling shader
	if(0 == window->gl_tex.program) 
	{
		// textured shader
		vert = create_shader(window, vert_shader_textured, GL_VERTEX_SHADER);
		frag = create_shader(window, frag_shader_textured, GL_FRAGMENT_SHADER);	

		window->gl_tex.program = glCreateProgram();
		glAttachShader(window->gl_tex.program, frag);
		glAttachShader(window->gl_tex.program, vert);
		glLinkProgram(window->gl_tex.program);	

		glGetProgramiv(window->gl_tex.program, GL_LINK_STATUS, &status);
		if (!status) 
		{			
			char log[1000];
			GLsizei len;
			glGetProgramInfoLog(window->gl_tex.program, 1000, &len, log);
			fprintf(stderr, "Error: linking:\n%*s\n", len, log);
			exit(1);
		}
		
		glUseProgram(window->gl_tex.program);

		// get shader attribute bind locations
		window->gl_tex.pos = glGetAttribLocation(window->gl_tex.program, "pos");
		window->gl_tex.col = glGetAttribLocation(window->gl_tex.program, "color");
		window->gl_tex.tex1 = glGetAttribLocation(window->gl_tex.program, "texcoord1");

		window->gl_tex.rotation_uniform =
			glGetUniformLocation(window->gl_tex.program, "model_matrix");
		window->gl_tex.view_uniform =
			glGetUniformLocation(window->gl_tex.program, "view");
		window->gl_tex.projection_uniform =
			glGetUniformLocation(window->gl_tex.program, "projection");	
		window->gl_tex.shader_loop_count = 
			glGetUniformLocation(window->gl_tex.program, "loop_count");
	}

	// flat grey pixel shader - for testing without using texture bandwidth
	vert = create_shader(window, vert_shader_textured, GL_VERTEX_SHADER);
	frag = create_shader(window, frag_shader_single_color, GL_FRAGMENT_SHADER);	

	window->gl_flat.program = glCreateProgram();
	glAttachShader(window->gl_flat.program, frag);
	glAttachShader(window->gl_flat.program, vert);
	glLinkProgram(window->gl_flat.program);	

	glGetProgramiv(window->gl_flat.program, GL_LINK_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetProgramInfoLog(window->gl_flat.program, 1000, &len, log);
		fprintf(stderr, "Error: linking:\n%*s\n", len, log);
		exit(1);
	}
	
	glUseProgram(window->gl_flat.program);

	// get shader attribute bind locations
	window->gl_flat.pos = glGetAttribLocation(window->gl_flat.program, "pos");
	window->gl_flat.col = glGetAttribLocation(window->gl_flat.program, "color");
	window->gl_flat.tex1 = glGetAttribLocation(window->gl_flat.program, "texcoord1");

	window->gl_flat.rotation_uniform =
		glGetUniformLocation(window->gl_flat.program, "model_matrix");
	window->gl_flat.view_uniform =
		glGetUniformLocation(window->gl_flat.program, "view");
	window->gl_flat.projection_uniform =
		glGetUniformLocation(window->gl_flat.program, "projection");	
	window->gl_flat.shader_loop_count = 
		glGetUniformLocation(window->gl_flat.program, "loop_count");

	// blur shader
	vert = create_shader(window, vert_shader_textured, GL_VERTEX_SHADER);
	frag = create_shader(window, frag_shader_blur_textured, GL_FRAGMENT_SHADER);	

	window->gl_tex_blur.program = glCreateProgram();
	glAttachShader(window->gl_tex_blur.program, frag);
	glAttachShader(window->gl_tex_blur.program, vert);
	glLinkProgram(window->gl_tex_blur.program);	

	glGetProgramiv(window->gl_tex_blur.program, GL_LINK_STATUS, &status);
	if (!status) {
		char log[1000];
		GLsizei len;
		glGetProgramInfoLog(window->gl_tex_blur.program, 1000, &len, log);
		fprintf(stderr, "Error: linking:\n%*s\n", len, log);
		exit(1);
	}
	
	glUseProgram(window->gl_tex_blur.program);

	// get shader attribute bind locations
	window->gl_tex_blur.pos = glGetAttribLocation(window->gl_tex_blur.program, "pos");
	window->gl_tex_blur.col = glGetAttribLocation(window->gl_tex_blur.program, "color");
	window->gl_tex_blur.tex1 = glGetAttribLocation(window->gl_tex_blur.program, "texcoord1");

	window->gl_tex_blur.rotation_uniform =
		glGetUniformLocation(window->gl_tex_blur.program, "model_matrix");
	window->gl_tex_blur.view_uniform =
		glGetUniformLocation(window->gl_tex_blur.program, "view");
	window->gl_tex_blur.projection_uniform =
		glGetUniformLocation(window->gl_tex_blur.program, "projection");	
	window->gl_tex_blur.shader_loop_count = 
		glGetUniformLocation(window->gl_tex_blur.program, "loop_count");
	window->gl_tex_blur.blur_radius = 
		glGetUniformLocation(window->gl_tex_blur.program, "blur_radius");	

	// load textures
	registerTexture(g_textureID, image_1k::width, image_1k::height, const_cast<char*>(image_1k::header_data), 2);

}

// Test scene: fullscreen quad with flat or blur shader
//------------------------------------------------------------------------------
void draw_simpleTexture(void *data, struct wl_callback *callback, uint32_t time_now)
{
	struct window *win = (window*)data;
	struct display *display = win->display;

	if(win->texture_flat_no_rotate)
	{
		glUseProgram(win->gl_flat.program);
	}
	else
	{
		glUseProgram(win->gl_tex_blur.program);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	glDisable(GL_CULL_FACE);	
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_textureID);

	struct wl_region *region;
	EGLint rect[4];
	EGLint buffer_age = 0;

	// callback and weston setup
	assert(win->callback == callback);
	win->callback = NULL;

	if (callback)
		wl_callback_destroy(callback);

	// timer for moving objects and timing frame
	float fps = calculate_fps(win, "simple_texture", time_now);
	
	if (display->swap_buffers_with_damage)
		eglQuerySurface(display->egl.dpy, win->egl_surface,
				EGL_BUFFER_AGE_EXT, &buffer_age);


	glViewport(0, 0, win->geometry.width, win->geometry.height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// orthographic projection matrix	
	float r=1.0f;
	float l=-1.0f;
	float t=1.0f;
	float b=-1.0f;
	float n=-1.0f;
	float f=1.0f;

	glm::mat4 _ortho_matrix = glm::ortho(l,r,b,t,n,f);
	glUniformMatrix4fv(win->gl_tex.projection_uniform, 1, GL_FALSE,
			   (GLfloat *) glm::value_ptr(_ortho_matrix));

	// view matrix
	glm::vec3 v_eye(0.0f, 0.0f, -1.0f);
	glm::vec3 v_center(0.0f, 0.0f, 0.0f);
	glm::vec3 v_up(0.0f, 1.0f, 0.0f);

	glm::mat4 view_matrix = glm::lookAt(v_eye, v_center, v_up);
	glUniformMatrix4fv(win->gl_tex.view_uniform, 1, GL_FALSE,
		(GLfloat *)glm::value_ptr(view_matrix));

	glm::mat4 model_matrix;

	// set the number of shader 'work' loops
#ifdef ANIMATED_BLUR	
	glUniform1f(win->gl_tex.shader_loop_count, 0); // or win->shortShader_loop_count if you want more work

	static bool flip=false;
	static float lastAngle = 0.0f;
	static float tex_scale_factor = 5.0f;
	if(lastAngle > angle) 
	{
		flip = !flip;
		printf("radius = %f\n", (lastAngle/360.0) * tex_scale_factor);
	}
	lastAngle = angle;

	if(flip)
		glUniform1f(win->gl_tex_blur.blur_radius, (1.0-(angle/360.0)) * tex_scale_factor);
	else
		glUniform1f(win->gl_tex_blur.blur_radius, (angle/360.0) * tex_scale_factor);
#else
	// 
	glUniform1f(win->gl_tex_blur.blur_radius, win->texture_fetch_radius);
#endif

	// set up buffers for draw
	glVertexAttribPointer(win->gl_tex.pos, 3, GL_FLOAT, GL_FALSE, 0, quad_verts);
	glVertexAttribPointer(win->gl_tex.col, 4, GL_FLOAT, GL_FALSE, 0, quad_colors);	
	glVertexAttribPointer(win->gl_tex.tex1,2, GL_FLOAT, GL_FALSE, 0, quad_texcoords);
	glEnableVertexAttribArray(win->gl_tex.pos);
	glEnableVertexAttribArray(win->gl_tex.col);
	glEnableVertexAttribArray(win->gl_tex.tex1);
			
	// fullscreen quad
	if(win->texture_flat_no_rotate)	{		
		model_matrix = glm::mat4();
	} else {
		//model_matrix = rotate(angle, 0.0f, 1.0f, 0.0f);
		model_matrix = glm::mat4();		
	}
	
	glUniformMatrix4fv(win->gl_tex.rotation_uniform, 1, GL_FALSE,
		(GLfloat *)glm::value_ptr(model_matrix));

	glDrawArrays(GL_TRIANGLES, 0, 6);

	// handle flips/weston
	g_TextRender.DrawDigits(fps, (void*)win, callback, time_now);

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
