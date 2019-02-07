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

#include "simple-dial.h"

#include "shaders.h" // quad_verts/etc
#include "draw-digits.h"

// textures
#include "needle.h"
#include "dialface.h"

// glm math library
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"


// setup code for dual dial display
//------------------------------------------------------------------------------
void initialize_simpleDial(window *window)
{
	GLuint frag, vert;
	GLint status;

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
		if (!status) {
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

	// load textures
	if(0==g_dialTexID) 
	{
		registerTexture(g_dialTexID, image_dial::width, image_dial::height, image_dial::header_data, 2);
	}
	
	registerTexture(g_needleTexID, image_needle::width, image_needle::height, image_needle::header_data, 1);
}


// draw the dual dial display
//------------------------------------------------------------------------------
void draw_simpleDial(void *data, struct wl_callback *callback, uint32_t time_now)
{
	struct window *win = (window*)data;
	struct display *display = win->display;

	static bool first_frame = true;
	static float left_dial_angle = 0.0f;
	static GLfloat angle = 0.0f;

	glUseProgram(win->gl_tex.program);

	glDisable(GL_CULL_FACE);	
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	
	glActiveTexture(GL_TEXTURE0);	
	glBindTexture(GL_TEXTURE_2D, g_dialTexID);

	struct wl_region *region;
	EGLint rect[4];
	EGLint buffer_age = 0;
	struct timeval tv;
	static struct timeval Startuptv;

	assert(win->callback == callback);
	win->callback = NULL;

	if (callback)
		wl_callback_destroy(callback);


	if(first_frame) 
	{
		// record the initial startup time
		// so that no matter what happens, our dials will spin smoothly
		// and independent of framerate or hitches
		gettimeofday(&Startuptv, NULL);
		first_frame = false;
	}

	// timer for moving objects and timing frame
	float fps = calculate_fps(win, "dials", time_now);


	// startup elapsed
	gettimeofday(&tv, NULL);
	long ElapsedTime = (tv.tv_sec-Startuptv.tv_sec) * 1000 + (tv.tv_usec-Startuptv.tv_usec) / 1000;	
	
	double bbR = 0.005;
	double bbL = 0.1;
	double cc = (ElapsedTime * bbR);
	double dd = (ElapsedTime * bbL);



	angle = (GLfloat) cc;
	if(angle > 360.0) {
		angle = angle - (int)(cc / 360.0) * 360;
		//printf("angle: %d\n", angle);
	}

	left_dial_angle = (float) dd;
	if (angle > 360.0)
		left_dial_angle = left_dial_angle - (int)(dd / 360.0) * 360;


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


	float aspect_ratio = (float) win->geometry.height / (float)win->geometry.width;
	float shrink = 3.0f / 4.0f;	// aspect ratio of the needle texture

	// set number of draw loops
	// set the number of shader 'work' loops
	glUniform1f(win->gl_tex.shader_loop_count, win->dialsShader_loop_count);


	glVertexAttribPointer(win->gl_tex.pos, 3, GL_FLOAT, GL_FALSE, 0, quad_verts);
	glVertexAttribPointer(win->gl_tex.col, 4, GL_FLOAT, GL_FALSE, 0, quad_colors);	
	glVertexAttribPointer(win->gl_tex.tex1,2, GL_FLOAT, GL_FALSE, 0, quad_texcoords);
	glEnableVertexAttribArray(win->gl_tex.pos);
	glEnableVertexAttribArray(win->gl_tex.col);
	glEnableVertexAttribArray(win->gl_tex.tex1);
			
		// left dial				
		glm::mat4 model_matrix(1.f);
		glm::mat4 identity_matrix(1.f);
		model_matrix = glm::translate(identity_matrix, glm::vec3(-0.5f, 0.0f, 0.0f));
		model_matrix = glm::scale(model_matrix, glm::vec3(aspect_ratio * shrink, 1.0f * shrink, 1.0f));				
		glUniformMatrix4fv(win->gl_tex.rotation_uniform, 1, GL_FALSE,
				   (GLfloat *) glm::value_ptr(model_matrix));	
		glDrawArrays(GL_TRIANGLES, 0, 6);

		// right dial
		model_matrix = glm::translate(identity_matrix, glm::vec3( 0.5f, 0.0f, 0.0f));
		model_matrix = glm::scale(model_matrix, glm::vec3(aspect_ratio * shrink, 1.0f * shrink, 1.0f));				
		glUniformMatrix4fv(win->gl_tex.rotation_uniform, 1, GL_FALSE,
				   (GLfloat *) glm::value_ptr(model_matrix));	
		glDrawArrays(GL_TRIANGLES, 0, 6);


		// left needle
		glUniform1f(win->gl_tex.shader_loop_count, 0);	// no work for needles, keep them normal
		model_matrix = glm::translate(identity_matrix, glm::vec3(-0.5f, 0.0f, 0.0f));
		model_matrix = glm::scale(model_matrix, glm::vec3(aspect_ratio * shrink, 1.0f * shrink, 1.0f));	
		model_matrix = glm::rotate(model_matrix, left_dial_angle*(3.14159265f/180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		model_matrix = glm::translate(model_matrix, glm::vec3(0.0f, 0.5f, 0.0f));
		model_matrix = glm::scale(model_matrix, glm::vec3(0.05f, 0.3f, 1.0f));	
		glUniformMatrix4fv(win->gl_tex.rotation_uniform, 1, GL_FALSE,
				   (GLfloat *) glm::value_ptr(model_matrix));	

		glBindTexture(GL_TEXTURE_2D, g_needleTexID);
		glDrawArrays(GL_TRIANGLES, 0, 6);


		// right needle
		model_matrix = glm::translate(identity_matrix, glm::vec3( 0.5f, 0.0f, 0.0f));
		model_matrix = glm::scale(model_matrix, glm::vec3(aspect_ratio * shrink, 1.0f * shrink, 1.0f));	
		model_matrix = glm::rotate(model_matrix, angle*(3.14159265f/180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		model_matrix = glm::translate(model_matrix, glm::vec3(0.0f, 0.5f, 0.0f));
		model_matrix = glm::scale(model_matrix, glm::vec3(0.05f, 0.3f, 1.0f));					
		glUniformMatrix4fv(win->gl_tex.rotation_uniform, 1, GL_FALSE,
				   (GLfloat *) glm::value_ptr(model_matrix));
		glDrawArrays(GL_TRIANGLES, 0, 6);

	// draw fps
	g_TextRender.DrawDigits(fps, (void*)win, callback, time_now);

	// do not call eglSwapBuffers?
	if(!win->no_swapbuffer_call)
	{
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
