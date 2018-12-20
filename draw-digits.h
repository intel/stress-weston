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
#ifndef __DRAWDIGITS_H__
#define __DRAWDIGITS_H__

#include <stdio.h>
#include <stdlib.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "shaders.h" // quad_texcoords/etc


// FPS onscreen counter
//-----------------------------------------------------------------------
#define digit_width (0.25f/20.0f)
#define digit_height 0.05f
#define digit_spacing 0.005f

// digit quad
static const GLfloat digit_verts[] = {
			 -1.00f, 1.00f, 0.0f,							// upper left
			 -1.00f, 1.0f - digit_height, 0.0f,				// lower left
			 -1.0f + digit_width, 1.0f - digit_height, 0.0f,// lower right	

			-1.00f, 1.00f, 0.0f,							// upper left
			-1.0f + digit_width, 1.0f - digit_height, 0.0f,	// lower right
			-1.0f + digit_width, 1.00f, 0.0f,				// upper right
};

// digit texel locations in the image
										// 0     1      2     3        4      5      6        7       8        9      .
static const GLfloat digit_uv_start[] = {0.0f, 0.04f, 0.08f, 0.125f, 0.170f, 0.22f, 0.250f, 0.300f, 0.345f, 0.390f, 0.480f};
static const GLfloat digit_uv_end[] = {0.05f, 0.09f, 0.135f, 0.170f, 0.220f, 0.25f, 0.295f, 0.345f, 0.390f, 0.430f, 0.500f};



class textRender
{
public:
	textRender();
	void InitializeDigits(window *window);
	void DrawDigits(float fps, void *data, struct wl_callback *callback, uint32_t time);	

private:
	float _last_fps;
};



#endif //__DRAWDIGITS_H__