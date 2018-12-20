OBJDIR=bin
TARG=stress-weston

ifndef TRACETOOL_LIB_PATH
T_LIB_PATH=
else
T_LIB_PATH=-L$(TRACETOOL_LIB_PATH)
endif

# flags for compile OpenGL, Wayland, etc
CFLAGS  += -I. -DUSE_FRAME_ENH -DNO_X11 -DUSE_WAY -DBENCH_FLAVOUR_GLES2 -Wno-write-strings
CFLAGS += -I$(WLD)/include -I../WAYLAND1_DEV/include -DUSE_WAYLAND1 -Wno-trigraphs
CFLAGS += -fstack-protector -fPIE -fPIC -O2 -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security
 

# flags for compiling IAS support on Clear linux
# remove if not needed
CFLAGS += -I /usr/include/libias-4

# -lm = fix for corei7-64-poky-linux/lib/libm.so.6: error adding symbols: DSO missing from command line
# -stdc+ = new/delete/constructors/etc
LIBS += -lm -lstdc++ -L../WAYLAND1_DEV/lib -lEGL -lGLESv2 -lwayland-client -lwayland-egl
LDFLAGS += -z noexecstack -z relro -z now -pie 

all: $(TARG)
SRCS = main.cpp shaders.cpp simple-dial.cpp draw-digits.cpp multi-draw.cpp single-draw.cpp batch-draw.cpp long-shader.cpp simple-texture.cpp ivi-application-protocol.c ias-shell-protocol.c xdg-shell-unstable-v5-protocol.c
OBJS = $(patsubst %.cpp,$(OBJDIR)/%.o,$(SRCS))


$(TARG):  $(OBJDIR) $(OBJS)
	$(CC) -o $@ $(OBJS) $(T_LIB_PATH) $(XLIBPATH) $(LIBS) $(LDFLAGS) $(CFLAGS)

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/%.o: %.cpp 
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo Cleaning up...
	@rm -rf $(OBJDIR)
	@rm $(TARG)
	@echo Done.

install:
	@cp $(TARG)  $(DESTDIR)/
	@cp params.txt  $(DESTDIR)/



