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
#ifndef __SHADERS_H__
#define __SHADERS_H__

#include <stdio.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "main.h"

const char* const vert_shader_single =
	"uniform mat4 model_matrix;\n"	
	"uniform mat4 view;\n"
	"uniform mat4 projection;\n"
	"uniform float loop_count;\n"

	"attribute vec4 pos;\n"
	"attribute vec4 color;\n"
	"attribute vec4 trans;\n"	

	"varying vec4 v_color;\n"
	"varying float v_loopcount;\n"

	"void main() {\n"
	"  mat4 translation;\n"
	"  translation = mat4(vec4(1.0, 0.0, 0.0, 0.0), vec4(0.0,1.0,0.0,0.0),vec4(0.0,0.0,1.0,0.0),vec4(trans.x,trans.y,trans.z,1.0));\n"
	"  vec4 world_pos = translation * model_matrix * pos;\n"
	"  vec4 eye_pos = view * world_pos;\n"
	"  vec4 clip_pos = projection * eye_pos;\n"
	"\n"
	"  gl_Position = clip_pos;\n"	
	"  v_color = vec4(vec3(color), 1.0f);\n"
	"  v_loopcount = loop_count;\n"
	"}\n";

const char* const frag_shader_short_loop =
	"precision mediump float;\n"
	"varying vec4 v_color;\n"
	"varying float v_loopcount;\n"
	"void main() {\n"
	"  gl_FragColor = v_color;\n"

	"  highp int loopcount = int(v_loopcount);\n"
	"  int count1 = 0;\n"
	"  for(int i=0; i<loopcount; i++)\n"
	"  {\n"
	"      gl_FragColor.b = gl_FragColor.b + 0.1;\n"
	"      if(count1 == loopcount) {\n"
	"          continue;\n"
	"      }\n"
	"      count1++;\n"
	"  }\n"

	"}\n";

// multi-draw shader
const char* const vert_shader_multi =
	"uniform mat4 model_matrix;\n"	
	"uniform mat4 view;\n"
	"uniform mat4 projection;\n"
	"uniform float loop_count;\n"

	"attribute vec4 pos;\n"
	"attribute vec4 color;\n"
	"attribute vec4 trans;\n"	

	"varying vec4 v_color;\n"
	"varying float v_loopcount;\n"

	"void main() {\n"
	"  vec4 world_pos = model_matrix * pos;\n"
	"  vec4 eye_pos = view * world_pos;\n"
	"  vec4 clip_pos = projection * eye_pos;\n"
	"\n"
	"  gl_Position = clip_pos;\n"	
	//"  gl_Position = projection * (rotation * pos);\n"
	"  v_color = vec4(vec3(color), 1.0f);\n"
	"  v_loopcount = loop_count;\n"
	"}\n";	


// textured shaders
const char* const vert_shader_textured =
"uniform mat4 model_matrix;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"uniform float loop_count;\n"
"uniform float blur_radius;\n"

"attribute vec4 pos;\n"
"attribute vec4 color;\n"
//"attribute vec4 trans;\n"
"attribute vec2 texcoord1;\n"

"varying vec4 v_color;\n"
"varying vec2 v_texcoord1;\n"
"varying float v_loopcount;\n"
"varying float v_blurradius;\n"

"void main() {\n"
"  vec4 world_pos = model_matrix * pos;\n"
"  vec4 eye_pos = view * world_pos;\n"
"  vec4 clip_pos = projection * eye_pos;\n"
"\n"
"  gl_Position = clip_pos;\n"
//"  gl_Position = projection * (rotation * pos);\n"
"  v_color = vec4(vec3(color), 1.0);\n"
"  v_texcoord1 = texcoord1;\n"
"  v_loopcount = loop_count;\n"
"  v_blurradius = blur_radius;\n"
"}\n";


const char* const frag_shader_textured =
	"precision mediump float;\n"
	"varying vec4 v_color;\n"
	"varying vec2 v_texcoord1;\n"
	"uniform sampler2D texSampler1;\n"
	"varying float v_loopcount;\n"
	"varying float v_blurradius;\n"

	"void main() {\n"
	"	gl_FragColor.b = 0.0f;\n"
	"  gl_FragColor = texture2D(texSampler1, v_texcoord1);\n"

	"  highp int loopcount = int(v_loopcount);\n"	
	"  int count1 = 0;\n"	
	"  for(int i=0; i<loopcount; i++)\n"
	"  {\n"
	"      gl_FragColor.r = gl_FragColor.r + 0.1;\n"
	"      if(count1 == loopcount) {\n"
	"          continue;\n"
	"      }\n"
	"      count1++;\n"
	"  }\n"
	"}\n";

// this is a really terrible 'blur' shader, but it does allow for
// us to easily test a variable number of texture fetches without
// writing a complex, and proper, blur shader
const char* const frag_shader_blur_textured =
	"precision highp float;\n"
	"varying vec4 v_color;\n"
	"varying vec2 v_texcoord1;\n"
	"uniform sampler2D texSampler1;\n"
	"varying float v_loopcount;\n"
	"varying float v_blurradius;\n"
	"\n"
	"void main() {\n"
	"  	gl_FragColor = texture2D(texSampler1, v_texcoord1);\n"
	"	float radius = v_blurradius;\n"
	"	float resolution = 1024.0;\n"
	"\n"
	"	float blur = radius/resolution;\n"	
	"	vec2 tc = v_texcoord1;\n"
	"	vec4 sum = vec4(0.0, 0.0, 0.0, 1.0);\n"
	"\n"
	" 	sum += texture2D(texSampler1, vec2(tc.x - 4.0*blur, tc.y - 4.0*blur)) * 0.0162162162;\n"
	" 	sum += texture2D(texSampler1, vec2(tc.x - 3.0*blur, tc.y - 3.0*blur)) * 0.0540540541;\n"
	" 	sum += texture2D(texSampler1, vec2(tc.x - 2.0*blur, tc.y - 2.0*blur)) * 0.1216216216;\n"
	" 	sum += texture2D(texSampler1, vec2(tc.x - 1.0*blur, tc.y - 1.0*blur)) * 0.1945945946;\n"
	"\n"
	" 	sum += texture2D(texSampler1, vec2(tc.x, tc.y)) * 0.2270270270;\n"
	"\n"
	" 	sum += texture2D(texSampler1, vec2(tc.x - 1.0*blur, tc.y - 1.0*blur)) * 0.1945945946;\n"	
	" 	sum += texture2D(texSampler1, vec2(tc.x - 2.0*blur, tc.y - 2.0*blur)) * 0.1216216216;\n"
	" 	sum += texture2D(texSampler1, vec2(tc.x - 3.0*blur, tc.y - 3.0*blur)) * 0.0540540541;\n"
	" 	sum += texture2D(texSampler1, vec2(tc.x - 4.0*blur, tc.y - 4.0*blur)) * 0.0162162162;\n"	
	"\n"
	" 	gl_FragColor = vec4(sum);\n"
	"\n"
	" 	// dummy tex sampling to add load\n"
	"	vec4 sum_discard = vec4(0.0, 0.0, 0.0, 1.0);\n"	
	"	float line_radius = radius / 4.0;\n"
	"	for(float i = -1.0*line_radius; i<line_radius; i=i+1.0)\n"
	"	{\n"
	"    	sum_discard += texture2D(texSampler1, vec2(tc.x + i*blur, tc.y)) * 1.0/(radius);\n"
	"	}\n"
	"	for(float j=-line_radius; j<line_radius; j=j+1.0)\n"
	"	{\n"
	"   	sum_discard += texture2D(texSampler1, vec2(tc.x, tc.y + j*blur)) * 1.0/(radius);\n"
	"	}\n"	
    " 	gl_FragColor = vec4(sum_discard);\n"
	
	"}\n";


const char* const frag_shader_long_shader_textured =
"precision mediump float;\n"
"varying vec4 v_color;\n"
"varying vec2 v_texcoord1;\n"
"varying float v_loopcount;\n"
"uniform sampler2D texSampler1;\n"
"void main() {\n"
"  vec4 color = texture2D(texSampler1, v_texcoord1);\n"
"  int count1 = 0;\n"
"  highp int loopcount = int(v_loopcount);\n"
"  for(int i=0; i<loopcount; i++)\n"
"  {\n"
"      color.b = color.b + 0.1;\n"
"      if(count1 == loopcount) {\n"
"          continue;\n"
"      }\n"
"      count1++;\n"
"  }\n"
"  gl_FragColor = color;\n"
"}\n";	

const char* const frag_shader_single_color =
"precision mediump float;\n"
"varying vec4 v_color;\n"
"varying vec2 v_texcoord1;\n"
"varying float v_loopcount;\n"
"uniform sampler2D texSampler1;\n"
"void main() {\n"
"  gl_FragColor = vec4(0.5, 0.5, 0.5, 1.0);\n"
"}\n";	


GLuint create_shader(window *window, const char *source, GLenum shader_type);
void registerTexture(GLuint& textureID, int image_width, int image_height, char* pimage_data, int alpha_style, bool digits=false);
void checkerBoardTexture(GLuint& textureID, int checkImageWidth, int checkImageHeight, int alpha_style);

// GEOMETRY
static const GLfloat pyramid_verts[] = {
			 0.0f, 1.0f, 0.0f,					// Top Of Triangle (Front)
			-1.0f,-1.0f, 1.0f,					// Left Of Triangle (Front)
			 1.0f,-1.0f, 1.0f,					// Right Of Triangle (Front)

			 0.0f, 1.0f, 0.0f,					// Top Of Triangle (Right)
			 1.0f,-1.0f, 1.0f,					// Left Of Triangle (Right)
			 1.0f,-1.0f, -1.0f,					// Right Of Triangle (Right)

			 0.0f, 1.0f, 0.0f,					// Top Of Triangle (Back)
			 1.0f,-1.0f, -1.0f,					// Left Of Triangle (Back)
			-1.0f,-1.0f, -1.0f,					// Right Of Triangle (Back)

			 0.0f, 1.0f, 0.0f,					// Top Of Triangle (Left)
			-1.0f,-1.0f,-1.0f,					// Left Of Triangle (Left)
			-1.0f,-1.0f, 1.0f,					// Right Of Triangle (Left)

			-1.0f, -1.0f, -1.0f,				// bottom
			 1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f,

			-1.0f, -1.0f,  1.0f,				// bottom
			 1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f,  1.0f,
			
};

static const GLfloat pyramid_colors[] = {
			1.0f,0.0f,0.0f,1.0f,						// Red
			0.0f,1.0f,0.0f,1.0f,						// Green
			0.0f,0.0f,1.0f,1.0f,						// Blue

			1.0f,0.0f,0.0f,1.0f,						// Red
			0.0f,0.0f,1.0f,1.0f,						// Blue
			0.0f,1.0f,0.0f,1.0f,						// Green

			1.0f,0.0f,0.0f,1.0f,						// Red
			0.0f,1.0f,0.0f,1.0f,						// Green
			0.0f,0.0f,1.0f,1.0f,						// Blue

			1.0f,0.0f,0.0f,1.0f,						// Red
			0.0f,0.0f,1.0f,1.0f,						// Blue
			0.0f,1.0f,0.0f,1.0f,						// Green
			
			1.0f,0.0f,0.0f,1.0f,						// Red
			0.0f,1.0f,0.0f,1.0f,						// Green
			0.0f,0.0f,1.0f,1.0f,						// Blue

			1.0f,0.0f,0.0f,1.0f,						// Red
			0.0f,1.0f,0.0f,1.0f,						// Green
			0.0f,0.0f,1.0f,1.0f,						// Blue
};



static const GLfloat quad_verts[] = {
			 -1.0f, 1.0f, 0.0f,					// upper left
			 -1.0f,-1.0f, 0.0f,					// lower left
			  1.0f,-1.0f, 0.0f,					// lower right	

			-1.0f, 1.0f, 0.0f,					// upper left
			 1.0f,-1.0f, 0.0f,					// lower right
			 1.0f, 1.0f, 0.0f,					// upper right
};


static const GLfloat quad_texcoords[] = {
			0.0f,0.0f,						// 
			0.0f,1.0f,						// 
			1.0f,1.0f,

			0.0f,0.0f,						// 
			1.0f,1.0f,						
			1.0f,0.0f,						// upper right
};

static const GLfloat quad_colors[] = {
			1.0f,0.0f,0.0f,1.0f,						// Red
			0.0f,1.0f,0.0f,1.0f,						// Green
			0.0f,0.0f,1.0f,1.0f,						// Blue

			1.0f,0.0f,0.0f,1.0f,						// Red
			0.0f,0.0f,1.0f,1.0f,						// Blue			
			0.0f,1.0f,0.0f,1.0f,						// Green
			
};



#endif