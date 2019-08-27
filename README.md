# stress_weston 
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

stress_weston is an open source project designed to create very specific GPU
workloads using OpenGL on top of the Weston compositor. These workloads can be
used to test the performance of systems. Due to the very specific nature of 
the workloads it can generate, it is very useful for testing GPU preemption 
capabilities as outlined in the ACRN hypervisor project. See more information 
about GPU preemption at https://projectacrn.github.io/

There preemption details are in Developer Guides -> 
High-Level Design Guides -> Emulated Devices -> GVT-g GPU Virtualization

This app consists of 4 highly configurable test scenes/workloads useful for 
evaluating pre-emption performance and inducing specific kinds of workloads
to test the performace of a system.


## Building

A makefile is provided with the distribution. It should work on most Yocto or
Clear linux installations. The sample compiles against:
1. Weston compositor 
2. Weston IAS package
3. GLM - OpenGL Math Library v0.9.8.4 
Make sure you have those packages or bundles installed. 
GLM is a source-only component and therefore must be located
in an appropriately named directory (./glm) in your source folder. 

### Clear Linux

The following steps will build stress-weston on Clear Linux:
```
$ sudo swupd bundle-add os-clr-on-clr software-defined-cockpit-dev devpkg-glm
$ wget https://cdn.download.clearlinux.org/releases/current/clear/x86_64/os/Packages/glm-dev-0.9.9.2-3.x86_64.rpm
$ git clone https://github.com/intel/stress-weston
$ cd stress-weston
$ make
```

Optional manual installation of glm:
$ sudo rpm -i glm-dev-0.9.9.2-3.x86_64.rpm --force --nodeps

## Configuration file

By default, if you provide no command line parameters, stress_weston will read 
the params.txt file in the local directory.

If you give a filename after the executable like this:
```
stress_weston myparams.txt
```
It will read and use the 'myparams.txt' file for settings. This is handy for 
keeping and switching between parameters files quickly.

The parameters file controls the startup parameters of the tests. You MUST
follow the format specified in the params.txt sample file. Don't re-arrange
parameters or delete/add any lines. Each parameter needs to be on its own line
and anything past the // comment marker is ignored.

Format of params line:
Line #    value  // comment

Parameters File Format:
1. Window width (in pixels)
2. Window height (in pixels)
3. Fullscreen mode (0=fullscreen, 1 = windowed mode)
4. Save per frame metrics into a comma seperated value file. The CVS filename is 
automatically generated with the current date/time stamp. It contains the frame
time for every frame seperated by comma. This is useful for automated testing
or making sure there are no single frame spikes.
5. Draw to an offscreen buffer (0=onscreen, 1=offscreen). If you render 
offscreen, then nothing will appear on the screen. 
6. Vsync on/off (0=vsync off, 1=vsync on)
7. eglSwapbuffers (0=do not call eglSwapBuffers, 1=normal rendering). If the 
app is directed not to call eglSwapbuffers, then no onscreen updates will 
happen.
8. Test scene to render. There are 5 different 'scenes' that have specific
workloads. Set this paramter to a value between 0 and 5.

0 = Dual dials scene. 2 spinning dials are rendered. Dummy per-pixel work might
 be performed by the pixel shader if indicated by parameter 16. The number in 
 parameter 16 specifies how many per-pixel loops with dummy math are performed.
 Paramter 16 is 0, then the dial faces will be completely black to indicate 
 there is no extra per-pixel work being done. If there is some per-pixel work, 
 then the dial faces will have a light red tint to indicate per-pixel work is 
 being done.

General comments about scenes 1,2, and 5:
In this mode, a 3D volume of pyramids are drawn in a grid. The number of 
pyramids in the grid is controlled by the paramters 11, 12, and 13 (X, Y, and Z
direction respectively). The rendering of the pyramids always proceeds from 
furthest back (z-axis) to the front so the maximum amount of rendering must 
take place (this avoids early-Z test rejection). It is good practice to keep 
the grid roughly box-shaped so that when adding more pyramids, they occupy 
roughly the same amount of screen space as opposed to a grid that stretches in 
only one or two directions. This helps the work scale more predictably if you 
add/remove pyramids. Each pyramid consists of 6 triangles.

The pixel shader used for rendering these pyramids also has an option to add 
some dummy, per-pixel math operations to add load. Parameter 15 controls the 
amount of extra per-pixel work done by the pixel shader when rendering each 
triangle of a pyramid. If parameter 15 is set to 0, then the pyramids are drawn
in pure per-vertex colors of red, green, blue. If parameter 15 is non-zero, 
then the pyramids will have a slight blue tint to indicate extra per-pixel work
is being done.

1 = Single Buffer Draw
The triangles for all the pyramids are created in a single vertex buffer and 
drawn in a single GL render command. This is useful for making sure a single 
draw command can be interrupted.

2 = Multiple Draw
A single vertex buffer is created that has only 1 pyramid in it. Each pyramid 
is drawn via a seperate gl render command after modifying the render location 
in the constant buffer.

5 = Group Draw
The triangles for all the pyramids are created into N vertex buffers and each 
rendered with a individual GL render call. Parameter 14 controls how many 
seperate vertex buffers/draw calls the pyramids should be broken into. 

For example, if paramters 11,12,13 are 6,6, and 6 - this means 216 pyramids 
will be drawn. If parameter 14 is set to 8, this means that 8 vertex buffers 
with 27 pyramids each will be created. The GL render command will be issued 8 
times total, 1 for each of the vertex buffers. If parameter 14 is set to 1, it
is the same as rendering the Single Buffer Draw scene. If it's set to the 
number of pyramids specified by parameters 11,12, and 13, then it is identical
to scene 2 - muliple draw.

3 = Full-screen quad with texture (and optional blur filter)
In this mode, a single full-screen quad (2 triangles that cover the entire 
screen) is drawn. A texture is applied to the quad, unless parameter 9 is set
to 1. If parameter 9 is set to 1, then no texture is drawn and the quad is 
simply drawn with a flat grey shader.

Parameter 10 controls the faux-blur shader. The faux-blur pixel shader simply
does a number of texture fetches of neighboring pixels and averages them to 
create a simple blur effect. The number of pixels it fetches in the X and Y 
directions from the center pixel is controlled by parameter 10. Ex: if 
parameter 10 is set to 0, then no surrounding pixels are sampled and no blur
effect is applied. If the parameter is set to 4, then 4 pixels in the X and Y
direction will be sampled, per-pixel, by the pixel shader. This test is very
useful for testing common post-processing workloads that do many texture
fetches/samples.

4 = Long, compute-intensive shader
In this mode, a single, full-screen quad is drawn with a rotating texture. Some
simple shader math operations are done per-pixel in a loop. The number of math
operations per pixel is controlled by parameter 17. This scene is very useful
for testing heavy compute shaders.

18 - the number of frames stress-weston should render before exiting. Setting
this value to 0 indicates it should run forever.


## Keys for controlling the parameters at runtime
If you have a keyboard plugged into your system, you can press these keys:

```
'c' - change to the next test in the suite
<esc> - exit the tests
```

On the pyramid scene tests:
```
'+' - add 5 more pyramids to each direction (x/y/z) of the grid being drawn
'-' - subtract 5 pyramids in each direction (x/y/z) of the grid being drawn
```

On the longshader test:
```
'+' - add 500 loops to each pixel shader invocation
'-' - subtract 500 loops from each pixel shader invocation
```

## Running more than one instance

It is easy to run more than one instance of the workload tests as there are 
no dependencies. My recommendation is to dump the output to null if you have 
more than one going:
```
stress_weston > /dev/null &
```

## Moving the output window
Weston does not give permission for an app to move itself. You need to use
the surfctrl app to move the app to different screens/locations.
surfctrl --surfid=XXXXXXX --pos=1920x0 
is a handy command to move something to a second monitor (if the first monitor
is running 1920x1080)








