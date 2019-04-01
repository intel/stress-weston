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
#include "draw-digits.h"

// Textures
#include "digits.h"
#include <iostream>
#include <sstream>
#include <algorithm>

// glm math library
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

GLuint g_digitTextureID=0;

//------------------------------------------------------------------------------
textRender::textRender()
{
	_last_fps = 0.0;
}

// Initialize the textures and other resources needed
//------------------------------------------------------------------------------
void textRender::InitializeDigits(window *window)
{
	registerTexture(g_digitTextureID, image_digits::width, image_digits::height, image_digits::header_data, 0);
}

// render routine to draw the digits in upper-left
//------------------------------------------------------------------------------
void textRender::DrawDigits(float fps, void *data, struct wl_callback *callback, uint32_t time)
{
	struct window *win = (window*)data;
	struct display *display = win->display;

	glUseProgram(win->gl_tex.program);

	glDisable(GL_CULL_FACE);	
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	// set the texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, g_digitTextureID); 	

	EGLint buffer_age = 0;

	assert(win->callback == callback);
	win->callback = NULL;

	if (callback)
		wl_callback_destroy(callback);

	if (display->swap_buffers_with_damage)
	{
		eglQuerySurface(display->egl.dpy, win->egl_surface,
				EGL_BUFFER_AGE_EXT, &buffer_age);
	}

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
	glm::vec3 v_eye(0.0f, 0.0f, 1.0f);
	glm::vec3 v_center(0.0f, 0.0f, 0.0f);
	glm::vec3 v_up(0.0f, 1.0f, 0.0f);

	glm::mat4 view_matrix = glm::lookAt(v_eye, v_center, v_up);
	glUniformMatrix4fv(win->gl_tex.view_uniform, 1, GL_FALSE,
		(GLfloat *)glm::value_ptr(view_matrix));


	// set the number of shader 'work' loops to 0
	glUniform1f(win->gl_tex.shader_loop_count, 0);


	// get the string representation of the digits
	std::ostringstream sstring;
	sstring << fps;
	std::string buffer(sstring.str());

	int numDigits = 0;

	for(int i=0;i<(int)buffer.length(); i++)
	{		
		if(buffer[i] == '.')
		{
			numDigits+=2;
			break;		
		}
		numDigits++;
	}


	// make a copy of the quad verts and modify those	
	const int number_of_verts_in_digit_quad = 18;	// 6*3
	const int number_of_uv_digits_in_digit_quad = 12; // 4*3

	GLfloat digits_verts[number_of_verts_in_digit_quad * numDigits];
	GLfloat digits_texcoods[number_of_uv_digits_in_digit_quad * numDigits];
	for(int i=0; i<number_of_verts_in_digit_quad * numDigits; i++)
	{
		digits_verts[i] = 0.0;
	}
	for(int i=0; i<number_of_uv_digits_in_digit_quad * numDigits; i++)
	{
		digits_texcoods[i] = 0.0;
	}

	int digit_index=0;
	int digit_vert_list_index=0;
	int digit_texcoord_list_index=0;
	


	bool after_decimal=false;
	int decimal_place = 0;
		
	for(int i=0; i<numDigits; i++)
	{
		// if digit is the decimal, mark it as such
		int digit = buffer[i]-'0';
		
		if(buffer[i] == '.')
			digit = 10;

		int digit_texcoord_source_index=0;
		if(1 >= decimal_place ) {			
			for(int x=0; x<number_of_verts_in_digit_quad; x=x+3)
			{			
				digits_verts[digit_vert_list_index+0] = digit_verts[x+0]+digit_index*(digit_width+digit_spacing);
				
				if(true==after_decimal)
				{
					decimal_place++;
					digits_verts[digit_vert_list_index+0] -= digit_width/2.0f;				
				}

				digits_verts[digit_vert_list_index+1] = digit_verts[x+1];
				digits_verts[digit_vert_list_index+2] = digit_verts[x+2];

				if(0.0f == quad_texcoords[digit_texcoord_source_index+0])
				{
					digits_texcoods[digit_texcoord_list_index+0] = digit_uv_start[digit];
				}
				else
				{
					digits_texcoods[digit_texcoord_list_index+0] = digit_uv_end[digit];	
				}
				digits_texcoods[digit_texcoord_list_index+1] = quad_texcoords[digit_texcoord_source_index+1];
				
				digit_vert_list_index+=3;
				digit_texcoord_list_index+=2;
				digit_texcoord_source_index+=2;				
			}			
		}

		if(true==after_decimal)
			break;

		if(10==digit)
		{
			after_decimal=true;			
		}

		digit_index++;

	}


	glVertexAttribPointer(win->gl_tex.pos, 3, GL_FLOAT, GL_FALSE, 0, digits_verts);
	glVertexAttribPointer(win->gl_tex.col, 4, GL_FLOAT, GL_FALSE, 0, quad_colors);	
	glVertexAttribPointer(win->gl_tex.tex1,2, GL_FLOAT, GL_FALSE, 0, digits_texcoods);
	glEnableVertexAttribArray(win->gl_tex.pos);
	glEnableVertexAttribArray(win->gl_tex.col);
	glEnableVertexAttribArray(win->gl_tex.tex1);
			
	// draw digit quad
	glm::mat4 _identity_matrix(1.f);
	glUniformMatrix4fv(win->gl_tex.rotation_uniform, 1, GL_FALSE,
			   (GLfloat *) glm::value_ptr(_identity_matrix));	

	glDrawArrays(GL_TRIANGLES, 0, 6*numDigits);

	glDisableVertexAttribArray(win->gl_tex.pos);
	glDisableVertexAttribArray(win->gl_tex.col);
	glDisableVertexAttribArray(win->gl_tex.tex1);
	glEnable(GL_DEPTH_TEST);
}

