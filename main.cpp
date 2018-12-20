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
#include <iostream>
#include <sstream>
#include <sys/stat.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "main.h"

#ifdef WESTON_EGL
#include "simple-egl-windowing.h"
#endif

#include "draw-digits.h"

// shaders
#include "shaders.h"

// globals
window  g_window;
textRender g_TextRender;
std::ofstream g_metricsfile;
bool g_recordMetrics = false;
DrawCases g_draw_case = simpleDial;
static bool g_Initalized = false;
unsigned int g_FramesToRender = 0;

// texture ids
GLuint g_textureID=0;
GLuint g_dialTexID=0;
GLuint g_needleTexID=0;

// pyramid geometry parameters
int x_count = 5;
int y_count = 5;
int z_count = 5;
int g_batchSize = 1;
GLfloat* pyramid_positions=NULL; 
GLfloat* pyramid_colors_single_draw=NULL; 
GLfloat* pyramid_transforms=NULL; 

// textures
#define checkImageWidth 64
#define checkImageHeight 64
GLubyte checkImage[checkImageHeight][checkImageWidth][4];

// scenes
#include "simple-dial.h"
#include "multi-draw.h"
#include "single-draw.h"
#include "long-shader.h"
#include "simple-texture.h"
#include "batch-draw.h"

// glm math library
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"


// parse out the digits of a string
//------------------------------------------------------------------------------
int safeParse(std::string& line, const int max_digits, const int minvalue=0)
{
	int value=0;
	char* end = NULL;

	value = (int) strtol(line.substr(0,max_digits).c_str(), &end, 10);

	if(value < 0)
	{
		value = 0;
	}

	if(value < minvalue)
	{
		value = minvalue;
	}

	return value;
}

// calculate the per-frame fps 
//------------------------------------------------------------------------------
float calculate_fps(window *win, char* test_name, uint32_t& time_now)
{
	static const uint32_t benchmark_interval = 1000;
	struct timeval tv;
	static float fps=0.0f;

	// saving to the a file
	static uint32_t* frametimes = NULL;
	static uint32_t* frameids = NULL;
	static uint32_t frametime_window_index=0;

	// timer related
	static uint64_t global_frameid = 0;
	static uint64_t prev_frame_timestamp = 0;
	static uint64_t now_timestamp = 0;
	static uint64_t now = 0;
	static uint64_t prev = 0;

	// get time now
	gettimeofday(&tv, NULL);
	now = tv.tv_sec * 1000000 + tv.tv_usec;
	
	// first frame setup
	if(0==global_frameid) { 
		prev = now;
	}	


	// one-time setup for first frame
	if((NULL==frametimes) && g_recordMetrics)
	{
		// allocate space for circulare frame time buffer
		frametimes = new uint32_t[FRAME_TIME_RECORDING_WINDOW_SIZE];
		frameids = new uint32_t[FRAME_TIME_RECORDING_WINDOW_SIZE];
		frametime_window_index=0;	

		gettimeofday(&tv, NULL);
		prev_frame_timestamp = tv.tv_sec * 1000000 + tv.tv_usec ; 

		// create unique metrics file name				
		std::string filename = "metrics_";

		time_t currDateTime = time(NULL);
		struct tm* tm = localtime(&currDateTime);

		// build filename for log using current date/time
		if(tm)
		{
			std::ostringstream year,mon,mday,hour,min,sec;
			year << tm->tm_year+1900;
			mon << tm->tm_mon+1;
			mday << tm->tm_mday;
			hour << tm->tm_hour;
			min << tm->tm_min;
			sec << tm->tm_sec;

			filename += year.str() + "-" + mon.str() + "-" + mday.str() + "__" + hour.str() + "-" + min.str() + "-" + sec.str() + ".cvs";
		}
		else
		{
			filename += "log";
		}

		// open new file
		printf("Saving metrics in file: %s\n", filename.c_str());
		g_metricsfile.open(filename.c_str(), std::ofstream::out ); 

		// possibly store scene/configuration settings here

		// add column headers
		g_metricsfile << "frame,\tmicroseconds (1e-6)\n";		
	}

	// if the frame metrics buffer is full, append it to the file
	if((frametime_window_index >= FRAME_TIME_RECORDING_WINDOW_SIZE-1) && g_recordMetrics)
	{
		printf("dumping metrics...\n");

		// dump to file
		if(FRAME_TIME_RECORDING_WINDOW_SIZE < global_frameid)
		{
			g_metricsfile << ",\n";
		}


		for(int i=0; i<FRAME_TIME_RECORDING_WINDOW_SIZE-2; i++)
		{
			g_metricsfile << frameids[i] << ",\t" << frametimes[i] << ",\n" ;
		}
		g_metricsfile << frameids[FRAME_TIME_RECORDING_WINDOW_SIZE-2] << ",\t" << frametimes[FRAME_TIME_RECORDING_WINDOW_SIZE-2] ;

		frametime_window_index = 0;
	}


	// store the time and frame id if we are recording metrics
	if(g_recordMetrics) 
	{		
		now_timestamp = tv.tv_sec * 1000000 + tv.tv_usec;

		frametimes[frametime_window_index] = (now_timestamp - prev_frame_timestamp);
		frameids[frametime_window_index] = global_frameid;

		prev_frame_timestamp = now_timestamp;
		frametime_window_index++;
	}

	// calculate delta since interval start
	uint64_t timeDelta = now - prev;

	// calculate fps if we have exceeded the benchmark interval 
	if (timeDelta > (benchmark_interval * 1000)) 
	{
		fps = (float) ((double)win->frames / ((double) timeDelta / 1000000.0));
		printf("%s: %d frames in %.4f seconds: %.4f fps. \n", 
				test_name,
		       	win->frames,
		       	timeDelta/1000000.0, 
		       	fps);
		prev = now;
		win->frames = 0;
		win->benchmark_time = now;

	}	
	time_now = now / 1000.0;

	// increment the global frame id
	// the global frame id never resets, starts from first app frame
	global_frameid++;			

	return fps;
}

// This function generates the gigantic draw buffers that have all the pyramids
// verts, colors, and transforms to make a single draw call
//------------------------------------------------------------------------------
void generate_pyramid_buffers()
{
	// allocate space for pyramid verts/colors/etc
	printf("Allocating mesh space...\n");
	if(pyramid_positions)
		delete [] pyramid_positions;
	if(pyramid_colors_single_draw)
		delete [] pyramid_colors_single_draw;
	if(pyramid_transforms)
		delete [] pyramid_transforms;

	pyramid_positions = new GLfloat[(3 * 18) * (x_count * y_count * z_count)];
	pyramid_colors_single_draw = new GLfloat[(4 * 18) * (x_count * y_count * z_count)];
	pyramid_transforms = new GLfloat[(3 * 18) * (x_count * y_count * z_count)];	


	// set up the pyramid transforms
	printf("Generating transforms...\n");

	int pyramid_index=0;
	// render back to front
	for(int z=z_count; z>=1; z--)
	{
		for(int x=1; x<=x_count; x++)
		{
			for(int y=1; y<=y_count; y++)
			{				
				for(unsigned int i=0;i<18;i++)
				{
					pyramid_transforms[3*(pyramid_index)+0] = (x-1.0f)*3.0f;
					pyramid_transforms[3*(pyramid_index)+1] = (y-1.0f)*3.0f;
					pyramid_transforms[3*(pyramid_index)+2] = (z-1.0f)*3.0f;

					//printf("(%.1f, %.1f, %.1f)\n",pyramid_transforms[3*(pyramid_index)+0], pyramid_transforms[3*(pyramid_index)+1], pyramid_transforms[3*(pyramid_index)+2]);
					pyramid_index++;
				}
			}
		}
	}

	// copy pyramid verts
	printf("Generating positions...\n");

	int source_index=0;
	for(int i=0; i<(x_count*y_count*z_count)*(18*3); i++)
	{
		pyramid_positions[i] = pyramid_verts[source_index];		

		source_index++;
		if(source_index==18*3)
			source_index=0;
	}

	// vert colors
	printf("Generating colors...\n");
	source_index=0;
	for(int i=0; i<(x_count*y_count*z_count)*(18*4); i++)
	{
		pyramid_colors_single_draw[i] = pyramid_colors[source_index];

		source_index++;
		if(source_index==18*4)
		{
			source_index=0;
		}
	}
}

// initialize/loading the gl resources for our scenes
//------------------------------------------------------------------------------
void init_gl(struct window *window)
{
	// initialize each of the scenes and utility 
	// classes that might need GL registration/setup
	initialize_simpleDial(window);

	initialize_singleDrawArrays(window);
	
	initialize_multiDrawArrays(window);

	initialize_simpleTexture(window);

	initialize_longShader(window);

	initialize_batchDrawArrays(window);

	g_TextRender.InitializeDigits(window);


	// create offscreen buffer if we params indicated to 
	// draw offscreen
	if(g_window.offscreen)
	{
		// create a framebuffer object
	    GLuint fboId = 0;

	    GLuint renderBufferWidth = g_window.geometry.width;
	    GLuint renderBufferHeight = g_window.geometry.height;	
	    glGenFramebuffers(1, &fboId);
	    glBindFramebuffer(GL_FRAMEBUFFER, fboId);


	    printf("glBindFramebuffer() = '0x%08x'\n", glGetError());
	    GLuint renderBuffer=0;
	    glGenRenderbuffers(1, &renderBuffer);
	    glBindRenderbuffer(GL_RENDERBUFFER, renderBuffer);
	    printf("glBindRenderbuffer() = '0x%08x'\n", glGetError());
	    glRenderbufferStorage(GL_RENDERBUFFER,
	                           GL_RGB565,
	                           renderBufferWidth,
	                           renderBufferHeight);
	    printf("glRenderbufferStorage() = '0x%08x'\n", glGetError());
	    glFramebufferRenderbuffer(GL_FRAMEBUFFER,
	                               GL_COLOR_ATTACHMENT0,
	                               GL_RENDERBUFFER,
	                               renderBuffer);

	    printf("glFramebufferRenderbuffer() = '0x%08x'\n", glGetError());
	    GLuint depthRenderbuffer;
	    glGenRenderbuffers(1, &depthRenderbuffer);
	    glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
	    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,     renderBufferWidth, renderBufferHeight);
	    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer);

	      // check FBO status
	    GLenum fbstatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	    if(fbstatus != GL_FRAMEBUFFER_COMPLETE) {
	    	printf("Problem with OpenGL framebuffer after specifying color render buffer: \n%x\n", fbstatus);
	    } else {
	        printf("FBO creation succedded\n");
		}

		// we must query for the format of the offscreen buffer
		// opengl es has very limited offscreen buffer formats
		GLint format = 0, type = 0;
		glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &format);
		glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &type);
		printf("Offscreen buffer - Format = 0x%x ",format);
		if(GL_RGB == format)
		{
			printf("GL_RGB");
		}
		printf("\nType = 0x%x",type);	// GL_RGB = 0x1907 in /usr/include/GLES2/gl2.h
		if(GL_UNSIGNED_SHORT_5_6_5 == type) {
			// GL_UNSIGNED_SHORT_5_6_5 = 0x8363
			printf("GL_UNSIGNED_SHORT_5_6_5");
		}
		printf("\n");
		

		// prime the buffers
		eglSwapBuffers(g_window.display->egl.dpy, g_window.egl_surface);
		eglSwapBuffers(g_window.display->egl.dpy, g_window.egl_surface);
		eglSwapBuffers(g_window.display->egl.dpy, g_window.egl_surface);
		printf("eglSwapBuffers() = '0x%08x'\n", glGetError());

		glClearColor(1.0,0.0,1.0,1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		eglSwapBuffers(g_window.display->egl.dpy, g_window.egl_surface);

	#if DISPLAY_OFFSCREEN_BUFFER_CONTENTS_FOR_TESTING
		// test code to validate we wrote to the output buffer
		// color of 1,0,1,1 = 0xf81f = 1111100000011111 in 5-6-5 format
		int size = 4 * renderBufferHeight * renderBufferWidth;
		unsigned char *data = new unsigned char[size];

		glReadPixels(0,0,renderBufferWidth,renderBufferHeight,GL_RGB, GL_UNSIGNED_SHORT_5_6_5, data);
		printf("glReadPixels() = '%s'\n", glGetError());

		for(int i=0; i<3; i++)
		{
			printf("(");
			for(int j=0; j<3; j++)
			{
				printf("0x%x,",data[3*i+j]);
			}
			printf(") ");
		}
		printf("\n");
	#endif
	}

	// set cross-scene defaults
	glEnable(GL_CULL_FACE);
    
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);    
    glDepthMask(GL_TRUE);
	
	glClearColor(0.0, 0.0, 0.0, 1.0);
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glViewport(0, 0, window->geometry.width, window->geometry.height);

	// create the single_draw pyramid buffers
	generate_pyramid_buffers();

	// textures
	glUseProgram(window->gl_tex.program);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	g_Initalized = true;	

}



// swap which scene we're drawing
//------------------------------------------------------------------------------
void swap_draw_case(window* win, DrawCases drawcase)
{
	switch(g_draw_case)
	{
		case singleDrawArrays:
			g_draw_case = multiDrawArrays;
			printf("Test case: multiDrawArrays: (%d x %d x %d) = %d pyramids\n", x_count, y_count, z_count, (x_count*y_count*z_count));
			break;

		case multiDrawArrays:
			g_draw_case = batchDrawArrays;
			printf("Test case: BatchDrawArrays: (%d x %d x %d) = %d pyramids in %d batches of %d draw calls\n", x_count, y_count, z_count, (x_count*y_count*z_count), g_batchSize, (x_count*y_count*z_count)/g_batchSize);				
			break;

		case simpleTexture:
			g_draw_case = longShader;
			printf("Test case: LongShader: %.0f loops\n", win->longShader_loop_count);
			break;

		case longShader:
			g_draw_case = simpleDial;
			printf("Test case: SimpleDial: %.0f loops\n", win->dialsShader_loop_count);	
			break;				

		case simpleDial:
			g_draw_case = singleDrawArrays;
			printf("Test case: SingleDrawArrays: (%d x %d x %d) = %d pyramids\n", x_count, y_count, z_count, (x_count*y_count*z_count));				
			break;

		case batchDrawArrays:
			g_draw_case = simpleTexture;			
			printf("Text case: SimpleTexture:\n");
			break;

		default:
			printf("Unknown test case\n");
	};
}


// Increase the amount of work being done for long shader and single/mulit shader
//-----------------------------------------------------------------------------
void add_shader_loops(DrawCases scene)
{		
	switch(scene)
	{
		case simpleDial:	
			g_window.dialsShader_loop_count += 25;
			printf("Per-pixel dials shader loop count = %.0f\n", g_window.dialsShader_loop_count);	
			break;

		case singleDrawArrays:
		case multiDrawArrays:
		case batchDrawArrays:
			g_window.shortShader_loop_count += 25;
			printf("Per-pixel pyramid shader loop count = %.0f\n", g_window.shortShader_loop_count);
			break;

		case longShader:
			g_window.longShader_loop_count += 50;
			printf("Per-pixel long shader loop count = %.0f\n", g_window.longShader_loop_count);
			break;			
		default:
			break;
	}
}

// decrease the amount of work being done for long shader and single/mulit shader
//-----------------------------------------------------------------------------
void shrink_shader_loops(DrawCases scene)
{
	switch(scene)
	{
		case simpleDial:
			g_window.dialsShader_loop_count -= 25;
			if( g_window.dialsShader_loop_count<0 )
			{
				g_window.dialsShader_loop_count=0;
			}
			printf("Per-pixel dials shader loop count = %.0f\n", g_window.dialsShader_loop_count);	
			break;


		case singleDrawArrays:
		case multiDrawArrays:
		case batchDrawArrays:		
			g_window.shortShader_loop_count -= 25;
			if(g_window.shortShader_loop_count<0) {
				g_window.shortShader_loop_count = 0;
			}
			printf("Per-pixel short shader loop count = %.0f\n", g_window.shortShader_loop_count);
			break;
		default:
		case longShader:	
			g_window.longShader_loop_count -= 50;
			if(g_window.longShader_loop_count<0) {
				g_window.longShader_loop_count = 0;
			}
			printf("Per-pixel long shader loop count = %.0f\n", g_window.longShader_loop_count);
			break;			
	}
}

// increase the number of pyramids drawing in the single/multi draw case
//-----------------------------------------------------------------------------
void add_pyramids()
{		
		x_count += 5;
		y_count += 5;
		z_count += 5;

		generate_pyramid_buffers();

		printf("Pyramid count = (%d x %d x %d) = %d \n", x_count, y_count, z_count, x_count*y_count*z_count);

}

// reduce the number of pyramids drawing in the single/multi draw case
//-----------------------------------------------------------------------------
void remove_pyramids()
{		
		x_count -= 5;
		y_count -= 5;
		z_count -= 5;

		if(x_count<=3)
			x_count=3;
		if(y_count<=3)
			y_count=3;
		if(z_count<=3)
			z_count=3;	
		
		generate_pyramid_buffers();
		printf("Pyramid count = (%d x %d x %d) = %d \n", x_count, y_count, z_count, x_count*y_count*z_count);				
}


// read the params.txt file to get all our running parameters
//------------------------------------------------------------------------------
int read_config_file(char* config_filename)
{
	char filename[] = "params.txt";
	struct stat statbuff;

	std::ifstream infile;

	printf("Reading config from: ");
	if(config_filename)
	{
		printf("%s\n", config_filename);
		int err = stat(config_filename, &statbuff);
		if(err)
		{	
			printf("Error opening %s parameters file\n", config_filename);
			exit(1);
		}	

		infile.open(config_filename);	
	} else {
		printf("params.txt\n");

		int err = stat(filename, &statbuff);
		if(err)
		{	
			printf("Error opening the params.txt parameters file\n");
			exit(1);
		}		
		infile.open("params.txt");			
	}

	// read window/buffer dimensions
	std::string line;
	const int max_digits = 5;
	std::getline(infile, line);		
	g_window.geometry.width = safeParse(line, max_digits); 
	std::getline(infile, line);		
	g_window.geometry.height = safeParse(line, max_digits);
	printf("Window dimensions = (%d,%d)\n", g_window.geometry.width, g_window.geometry.height );	

	// run fullscreen?
	std::getline(infile, line);		
	g_window.fullscreen = safeParse(line, 1);
	printf("Window running fullscreen = %s\n", (g_window.fullscreen) ? "true" : "false" );

	// record metrics to a file?
	std::getline(infile, line);		
	g_recordMetrics = safeParse(line, 1);
	printf("Record metrics = %s\n", (g_recordMetrics) ? "true" : "false" );		

	// run offscreen
	std::getline(infile, line);		
	g_window.offscreen = safeParse(line, 1);
	printf("draw offscreen = %s\n", (g_window.offscreen) ? "true" : "false" );		

	// Enable vsync
	std::getline(infile, line);		
	g_window.frame_sync = safeParse(line, 1);
	printf("vsync = %d\n", g_window.frame_sync );		

	// Do not call swap buffers?
	std::getline(infile, line);		
	g_window.no_swapbuffer_call = safeParse(line, 1);
	printf("no_swapbuffer_call = %d\n", g_window.no_swapbuffer_call );			

	// which scene to draw
	std::getline(infile, line);
	g_draw_case = (DrawCases) safeParse(line, 1);

	switch (g_draw_case)
	{
	case multiDrawArrays:
		printf("Scene: multiDrawArrays\n");
		break;
	case singleDrawArrays:
		printf("Scene: singleDrawArrays\n");
		break;		
	case simpleTexture:
		printf("Scene: simpleTexture - 2k\n");
		break;
	case longShader:
		printf("Scene: longShader\n");
		break;
	case simpleDial:
		printf("Scene: simpleDial\n");
		break;
    	case batchDrawArrays:
		printf("Scene: batchDrawArrays\n");
		break;    		

	default:
		printf("Scene not supported, defaulting to dials\n");
		g_draw_case = simpleDial;
	}

	// use the flat grey shader for the texture scene?
	std::getline(infile, line);		
	g_window.texture_flat_no_rotate = safeParse(line, 1);
	printf("Texture scene using simple shader = %s\n", (g_window.texture_flat_no_rotate) ? "true" : "false" );		

	// if not using the flat gray shader on the texture scene, what is the 
	// blur kernel radius
	std::getline(infile, line);
	g_window.texture_fetch_radius = (float)safeParse(line, max_digits);
	if(g_window.texture_fetch_radius < 1) 
	{
		g_window.texture_fetch_radius = 1;
	}
	printf("Texture scene using fetch radius of = %.0f\n", g_window.texture_fetch_radius);				

	// pyramid scene counts
	std::getline(infile, line);
	x_count = safeParse(line, max_digits, 1);
	std::getline(infile, line);
	y_count = safeParse(line, max_digits, 1);
	std::getline(infile, line);
	z_count = safeParse(line, max_digits, 1);
	std::getline(infile, line);
	g_batchSize = safeParse(line, max_digits, 1);

	printf("pyramid scenes dimensions: %dx%dx%d ", x_count, y_count, z_count);
	if(batchDrawArrays != g_draw_case)
	{
		printf("\n");
	} else {
		printf("in %d batches\n",  g_batchSize);
	}

	// pyramid shader loop count
	std::getline(infile, line);
	g_window.shortShader_loop_count = (float) safeParse(line, max_digits);		
	printf("pyramid shader loop iterations: %.0f\n", g_window.shortShader_loop_count);

	// dials shader loop count
	std::getline(infile, line);
	g_window.dialsShader_loop_count = (float) safeParse(line, max_digits);		
	printf("dials scene shader loop iterations: %.0f\n", g_window.dialsShader_loop_count);		

	// read the long/fullscreen quad shader loop count
	std::getline(infile, line);
	g_window.longShader_loop_count = (float) safeParse(line, max_digits);
	printf("long shader loop iterations: %.0f\n", g_window.longShader_loop_count);

	// read number of frames to render
	std::getline(infile, line);
	g_FramesToRender = (float) safeParse(line, max_digits);
	if(0==g_FramesToRender)
	{
		printf("Number of frames to render: <infinite>\n");
	}
	else
	{
		printf("Number of frames to render: %d\n", g_FramesToRender);
	}
	return 0;
}

// main
//------------------------------------------------------------------------------
int main(int argc, char **argv)
{
	struct sigaction sigint;
	struct display display = { 0 };
	
	int ret = 0;
	struct output *iter, *next;

	memset(&g_window, 0, sizeof(g_window) );

	g_window.display = &display;
	display.window = &g_window;
	g_window.geometry.width  = WINDOW_WIDTH;	// default window dim
	g_window.geometry.height = WINDOW_HEIGHT;	// default window dim
	g_window.window_size = g_window.geometry;
	g_window.buffer_size = 32;	// 32 bpp
	g_window.frame_sync = 0;
	g_window.fullscreen = 0;
	g_recordMetrics = false;

	// read the config file
	char* config_filename = NULL;
	if(argc){
		config_filename = argv[1];
	}
	read_config_file(config_filename);
	
	// set up window
	display.display = wl_display_connect(NULL);
	assert(display.display);
	wl_list_init(&display.output_list);

	display.registry = wl_display_get_registry(display.display);
	wl_registry_add_listener(display.registry,
				 &registry_listener, &display);

	wl_display_dispatch(display.display);


	// initialize GL
	init_egl(&display, &g_window);
	create_surface(&g_window);
	init_gl(&g_window);

	display.cursor_surface =
		wl_compositor_create_surface(display.compositor);

	sigint.sa_handler = signal_int;
	sigemptyset(&sigint.sa_mask);
	sigint.sa_flags = SA_RESETHAND;
	sigaction(SIGINT, &sigint, NULL);


	// prime the egl swap buffers 
	// not sure why this is necissary - shouldn't be
	if(!g_window.no_swapbuffer_call) {
		eglSwapBuffers(display.egl.dpy, g_window.egl_surface);
		eglSwapBuffers(display.egl.dpy, g_window.egl_surface);
		eglSwapBuffers(display.egl.dpy, g_window.egl_surface);
	} else {
		printf("Skipping all eglSwapBuffer calls\n");
	}


	/* The mainloop here is a little subtle.  Redrawing will cause
	 * EGL to read events so we can just call
	 * wl_display_dispatch_pending() to handle any events that got
	 * queued up as a side effect. */
	 unsigned int loop_count = 0;
	while (running && ret != -1) {
		wl_display_dispatch_pending(display.display);

		if(0!=g_FramesToRender)
		{
			if(loop_count > g_FramesToRender){
				running = 0;
			}
		}
		loop_count++;

		switch(g_draw_case)
		{
			case singleDrawArrays:
				draw_singleDrawArrays(&g_window, NULL, 0);
				break;

			case multiDrawArrays:
				draw_multiDrawArrays(&g_window, NULL, 0);
				break;
			case simpleTexture:
				draw_simpleTexture(&g_window, NULL, 0);
				break;

			case simpleDial:
				draw_simpleDial(&g_window, NULL, 0);
				break;

			case longShader:
				draw_longShader(&g_window, NULL, 0);
				break;

			case batchDrawArrays:
				draw_batchDrawArrays(&g_window, NULL, 0);
				break;

			default:
				printf("Invalid draw case\n");
				assert(0);
				break;
		}
	}

	fprintf(stderr, "stress-weston exiting\n");

	// shutdown all weston resources	
	destroy_surface(&g_window);
	fini_egl(&display);

	wl_surface_destroy(display.cursor_surface);
#ifdef WL_CURSOR_LIB_ERROR	
	if (display.cursor_theme)
		wl_cursor_theme_destroy(display.cursor_theme);
#endif
	if (display.shell)
		xdg_shell_destroy(display.shell);

	if (display.ivi_application)
		ivi_application_destroy(display.ivi_application);

	if (display.ias_shell) {
		ias_shell_destroy(display.ias_shell);
	}

	if (display.compositor)
		wl_compositor_destroy(display.compositor);

	wl_registry_destroy(display.registry);

	wl_list_for_each_safe(iter, next, &display.output_list, link) {
		wl_list_remove(&iter->link);
		free(iter);
	}

	wl_display_flush(display.display);
	wl_display_disconnect(display.display);

	// close the frame metrics file
	if(g_recordMetrics) {
		g_metricsfile.close();
	}

	return 0;
}

