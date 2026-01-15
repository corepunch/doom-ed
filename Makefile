# Makefile for DOOM-ED
# Based on Xcode project configuration

# Compiler and flags
CC = gcc
CFLAGS = -Wall -std=gnu17
LDFLAGS = -lm

# Platform detection
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
  # macOS
  HOMEBREW_INCLUDES = -I/opt/homebrew/include -I/usr/local/opt/cglm/include
  GL_CFLAGS = -DGL_SILENCE_DEPRECATION
  GL_LDFLAGS = -framework OpenGL
  SDL_CFLAGS = $(shell pkg-config --cflags sdl2 2>/dev/null || echo "$(HOMEBREW_INCLUDES)")
  SDL_LDFLAGS = $(shell pkg-config --libs sdl2 2>/dev/null || echo "-L/opt/homebrew/lib -lSDL2")
  CGLM_CFLAGS = $(shell pkg-config --cflags cglm 2>/dev/null || echo "$(HOMEBREW_INCLUDES)")
  CGLM_LDFLAGS = $(shell pkg-config --libs cglm 2>/dev/null || echo "")
else
  # Linux
  GL_CFLAGS = -DGL_SILENCE_DEPRECATION -D__LINUX__
  GL_LDFLAGS = -lGL
  SDL_CFLAGS = $(shell pkg-config --cflags sdl2)
  SDL_LDFLAGS = $(shell pkg-config --libs sdl2)
  CGLM_CFLAGS = $(shell pkg-config --cflags cglm 2>/dev/null || echo "")
  CGLM_LDFLAGS = $(shell pkg-config --libs cglm 2>/dev/null || echo "-lcglm")
endif

# Combine all flags
CFLAGS += $(GL_CFLAGS) $(SDL_CFLAGS) $(CGLM_CFLAGS)
LDFLAGS += $(GL_LDFLAGS) $(SDL_LDFLAGS) $(CGLM_LDFLAGS)

# Directories
MAPVIEW_DIR = mapview
EDITOR_DIR = editor
DOOM_DIR = doom
HEXEN_DIR = hexen
BUILD_DIR = build
TESTS_DIR = tests
UI_DIR = ui

# Source files
MAPVIEW_SRCS = $(MAPVIEW_DIR)/bsp.c \
               $(MAPVIEW_DIR)/collision.c \
               $(MAPVIEW_DIR)/floor.c \
               $(MAPVIEW_DIR)/gamefont.c \
               $(MAPVIEW_DIR)/input.c \
               $(MAPVIEW_DIR)/main.c \
               $(MAPVIEW_DIR)/renderer.c \
               $(MAPVIEW_DIR)/sky.c \
               $(MAPVIEW_DIR)/sprites.c \
               $(MAPVIEW_DIR)/texture.c \
               $(MAPVIEW_DIR)/things.c \
               $(MAPVIEW_DIR)/triangulate.c \
               $(MAPVIEW_DIR)/wad.c \
               $(MAPVIEW_DIR)/walls.c \
               $(MAPVIEW_DIR)/wi_stuff.c

EDITOR_SRCS = $(EDITOR_DIR)/editor_draw.c \
              $(EDITOR_DIR)/editor_input.c \
              $(EDITOR_DIR)/editor_sector.c \
              $(EDITOR_DIR)/radial_menu.c \
              $(EDITOR_DIR)/windows/desktop.c \
              $(EDITOR_DIR)/windows/game.c \
              $(EDITOR_DIR)/windows/inspector/insp_line.c \
              $(EDITOR_DIR)/windows/inspector/insp_sector.c \
              $(EDITOR_DIR)/windows/inspector/insp_thing.c \
              $(EDITOR_DIR)/windows/inspector/insp_vertex.c \
              $(EDITOR_DIR)/windows/perfcounter.c \
              $(EDITOR_DIR)/windows/project.c \
              $(EDITOR_DIR)/windows/statbar.c \
              $(EDITOR_DIR)/windows/texatlas.c \
              $(EDITOR_DIR)/windows/things.c \
              $(EDITOR_DIR)/windows/tray.c \
              $(EDITOR_DIR)/windows/sprite.c

# DOOM files (only headers are used, .c files excluded per Xcode config)
# These are just listed for reference, not compiled
DOOM_HEADERS = $(DOOM_DIR)/d_englsh.h \
               $(DOOM_DIR)/d_think.h \
               $(DOOM_DIR)/info.h \
               $(DOOM_DIR)/m_fixed.h \
               $(DOOM_DIR)/p_mobj.h \
               $(DOOM_DIR)/sounds.h

# HEXEN files (all included per Xcode config)
HEXEN_SRCS = $(HEXEN_DIR)/actions.c \
             $(HEXEN_DIR)/hu_stuff.c \
             $(HEXEN_DIR)/info.c

# Object files
MAPVIEW_OBJS = $(MAPVIEW_SRCS:$(MAPVIEW_DIR)/%.c=$(BUILD_DIR)/mapview/%.o)
EDITOR_OBJS = $(EDITOR_SRCS:$(EDITOR_DIR)/%.c=$(BUILD_DIR)/editor/%.o)
HEXEN_OBJS = $(HEXEN_SRCS:$(HEXEN_DIR)/%.c=$(BUILD_DIR)/hexen/%.o)

# Libraries (libgoldie-ui built via ui/Makefile)
ifeq ($(UNAME_S),Darwin)
  LIBGOLDIE = $(BUILD_DIR)/libgoldie-ui.dylib
else
  LIBGOLDIE = $(BUILD_DIR)/libgoldie-ui.so
endif

# All object files for main executable
OBJS = $(MAPVIEW_OBJS) $(EDITOR_OBJS) $(HEXEN_OBJS)

# Targets
.PHONY: all clean test triangulate_test bsp_test libgoldie

all: libgoldie mapview

# libgoldie-ui library (built via ui/Makefile)
libgoldie: $(LIBGOLDIE)

$(LIBGOLDIE):
	@echo "Building libgoldie-ui via ui/Makefile..."
	@$(MAKE) -C $(UI_DIR) all

# mapview executable (main executable)
mapview: $(OBJS) $(LIBGOLDIE)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) $(LIBGOLDIE) $(LDFLAGS) -o doom-ed
	@echo "Built doom-ed executable"

# Legacy target name (kept for compatibility)
doom-ed: mapview

# Object file rules
$(BUILD_DIR)/mapview/%.o: $(MAPVIEW_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I. -c $< -o $@

$(BUILD_DIR)/editor/%.o: $(EDITOR_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I. -c $< -o $@

$(BUILD_DIR)/editor/windows/%.o: $(EDITOR_DIR)/windows/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I. -c $< -o $@

$(BUILD_DIR)/editor/windows/inspector/%.o: $(EDITOR_DIR)/windows/inspector/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I. -c $< -o $@

$(BUILD_DIR)/hexen/%.o: $(HEXEN_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I. -c $< -o $@

# Test targets
test: triangulate_test bsp_test
	@echo "=== Running all tests ==="
	@./triangulate_test
	@./bsp_test

triangulate_test: $(TESTS_DIR)/triangulate_test.c $(MAPVIEW_DIR)/triangulate.c
	$(CC) -DTEST_MODE -o $@ $^ -I. -lm

bsp_test: $(TESTS_DIR)/bsp_test.c
	$(CC) -o $@ $^ -I. -lm

# Clean
clean:
	-@if [ -f $(UI_DIR)/Makefile ]; then $(MAKE) -C $(UI_DIR) clean; fi
	rm -rf $(BUILD_DIR) 
	-rm -f triangulate_test bsp_test doom-ed
	-test -f mapview && rm -f mapview || true
	rm -f $(MAPVIEW_DIR)/*.o $(EDITOR_DIR)/*.o $(EDITOR_DIR)/windows/*.o $(EDITOR_DIR)/windows/inspector/*.o
	rm -f $(HEXEN_DIR)/*.o $(DOOM_DIR)/*.o
