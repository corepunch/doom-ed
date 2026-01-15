# Makefile for DOOM-ED
# Based on Xcode project configuration

# Compiler and flags
CC = gcc
CFLAGS = -Wall -std=gnu17
LDFLAGS = -lm

# Platform detection
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
  # macOS (arm64 and x86_64)
  GL_CFLAGS = -DGL_SILENCE_DEPRECATION
  GL_LDFLAGS = -framework OpenGL
  SDL_CFLAGS = -I/opt/homebrew/include -I/usr/local/include
  SDL_LDFLAGS = -L/opt/homebrew/lib -L/usr/local/lib -lSDL2
  CGLM_CFLAGS = -I/opt/homebrew/include -I/usr/local/opt/cglm/include
  CGLM_LDFLAGS = # cglm is header-only on macOS
else
  # Linux
  GL_CFLAGS = -DGL_SILENCE_DEPRECATION -D__LINUX__
  GL_LDFLAGS = -lGL
  SDL_CFLAGS = -I/usr/include -I/usr/local/include
  SDL_LDFLAGS = -L/usr/lib -L/usr/local/lib -lSDL2
  CGLM_CFLAGS = -I/usr/include -I/usr/local/include
  CGLM_LDFLAGS = -lcglm
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

# Libraries (libgoldieui built via ui/Makefile)
# Note: ui/Makefile must build the library to ui/build/lib/ directory
ifeq ($(UNAME_S),Darwin)
  LIBGOLDIE = $(UI_DIR)/build/lib/libgoldieui.dylib
else
  LIBGOLDIE = $(UI_DIR)/build/lib/libgoldieui.so
endif

# All object files for main executable
OBJS = $(MAPVIEW_OBJS) $(EDITOR_OBJS) $(HEXEN_OBJS)

# Targets
.PHONY: all clean test triangulate_test bsp_test libgoldie

all: libgoldie mapview

# libgoldieui library (built via ui/Makefile)
libgoldie: $(LIBGOLDIE)

$(LIBGOLDIE):
	@echo "Building libgoldieui via ui/Makefile..."
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
