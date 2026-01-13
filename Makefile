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
  GL_CFLAGS = -DGL_SILENCE_DEPRECATION
  GL_LDFLAGS = -framework OpenGL
  SDL_CFLAGS = $(shell pkg-config --cflags sdl2 2>/dev/null || echo "-I/opt/homebrew/include -I/usr/local/opt/cglm/include")
  SDL_LDFLAGS = $(shell pkg-config --libs sdl2 2>/dev/null || echo "-L/opt/homebrew/lib -lSDL2")
  CGLM_CFLAGS = $(shell pkg-config --cflags cglm 2>/dev/null || echo "-I/opt/homebrew/include -I/usr/local/opt/cglm/include")
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
DOOM_DIR = doom
HEXEN_DIR = hexen
BUILD_DIR = build

# Source files
MAPVIEW_SRCS = $(MAPVIEW_DIR)/bsp.c \
               $(MAPVIEW_DIR)/collision.c \
               $(MAPVIEW_DIR)/editor_draw.c \
               $(MAPVIEW_DIR)/editor_input.c \
               $(MAPVIEW_DIR)/editor_sector.c \
               $(MAPVIEW_DIR)/floor.c \
               $(MAPVIEW_DIR)/font.c \
               $(MAPVIEW_DIR)/icons.c \
               $(MAPVIEW_DIR)/input.c \
               $(MAPVIEW_DIR)/main.c \
               $(MAPVIEW_DIR)/radial_menu.c \
               $(MAPVIEW_DIR)/renderer.c \
               $(MAPVIEW_DIR)/sky.c \
               $(MAPVIEW_DIR)/sprites.c \
               $(MAPVIEW_DIR)/texture.c \
               $(MAPVIEW_DIR)/things.c \
               $(MAPVIEW_DIR)/triangulate.c \
               $(MAPVIEW_DIR)/wad.c \
               $(MAPVIEW_DIR)/walls.c \
               $(MAPVIEW_DIR)/wi_stuff.c \
               $(MAPVIEW_DIR)/window.c \
               $(MAPVIEW_DIR)/windows/console.c \
               $(MAPVIEW_DIR)/windows/desktop.c \
               $(MAPVIEW_DIR)/windows/game.c \
               $(MAPVIEW_DIR)/windows/inspector/insp_line.c \
               $(MAPVIEW_DIR)/windows/inspector/insp_sector.c \
               $(MAPVIEW_DIR)/windows/inspector/insp_thing.c \
               $(MAPVIEW_DIR)/windows/inspector/insp_vertex.c \
               $(MAPVIEW_DIR)/windows/perfcounter.c \
               $(MAPVIEW_DIR)/windows/project.c \
               $(MAPVIEW_DIR)/windows/statbar.c \
               $(MAPVIEW_DIR)/windows/texatlas.c \
               $(MAPVIEW_DIR)/windows/things.c \
               $(MAPVIEW_DIR)/windows/tray.c

# DOOM files (excluding actions.c, hu_stuff.c, info.c per Xcode config)
DOOM_SRCS = $(DOOM_DIR)/d_englsh.h \
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
HEXEN_OBJS = $(HEXEN_SRCS:$(HEXEN_DIR)/%.c=$(BUILD_DIR)/hexen/%.o)

# All object files for main executable
OBJS = $(MAPVIEW_OBJS) $(HEXEN_OBJS)

# Targets
.PHONY: all clean test triangulate_test bsp_test

all: doom-ed

# Main executable
doom-ed: $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

# Object file rules
$(BUILD_DIR)/mapview/%.o: $(MAPVIEW_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(MAPVIEW_DIR) -I$(DOOM_DIR) -I$(HEXEN_DIR) -c $< -o $@

$(BUILD_DIR)/mapview/windows/%.o: $(MAPVIEW_DIR)/windows/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(MAPVIEW_DIR) -I$(DOOM_DIR) -I$(HEXEN_DIR) -c $< -o $@

$(BUILD_DIR)/mapview/windows/inspector/%.o: $(MAPVIEW_DIR)/windows/inspector/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(MAPVIEW_DIR) -I$(DOOM_DIR) -I$(HEXEN_DIR) -c $< -o $@

$(BUILD_DIR)/hexen/%.o: $(HEXEN_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(MAPVIEW_DIR) -I$(DOOM_DIR) -I$(HEXEN_DIR) -c $< -o $@

# Test targets
test: triangulate_test bsp_test
	@echo "=== Running all tests ==="
	@./triangulate_test
	@./bsp_test

triangulate_test: $(MAPVIEW_DIR)/triangulate_test.c $(MAPVIEW_DIR)/triangulate_standalone.c
	$(CC) -DTEST_MODE -o $@ $^ -I$(MAPVIEW_DIR) -lm

bsp_test: $(MAPVIEW_DIR)/bsp_test.c
	$(CC) -o $@ $^ -I$(MAPVIEW_DIR) -lm

# Clean
clean:
	rm -rf $(BUILD_DIR) triangulate_test bsp_test doom-ed
	rm -f $(MAPVIEW_DIR)/*.o $(MAPVIEW_DIR)/windows/*.o $(MAPVIEW_DIR)/windows/inspector/*.o
	rm -f $(HEXEN_DIR)/*.o $(DOOM_DIR)/*.o
