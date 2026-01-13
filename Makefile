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
DOOM_DIR = doom
HEXEN_DIR = hexen
UI_FRAMEWORK_DIR = ui_framework
BUILD_DIR = build
TESTS_DIR = tests

# UI Framework source files
UI_FRAMEWORK_SRCS = $(UI_FRAMEWORK_DIR)/ui_window.c \
                    $(UI_FRAMEWORK_DIR)/ui_sdl.c \
                    $(UI_FRAMEWORK_DIR)/commctl/commctl.c

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
HEXEN_OBJS = $(HEXEN_SRCS:$(HEXEN_DIR)/%.c=$(BUILD_DIR)/hexen/%.o)
UI_FRAMEWORK_OBJS = $(UI_FRAMEWORK_SRCS:$(UI_FRAMEWORK_DIR)/%.c=$(BUILD_DIR)/ui_framework/%.o)

# UI Framework library
UI_FRAMEWORK_LIB = $(BUILD_DIR)/libui_framework.a

# All object files for main executable
OBJS = $(MAPVIEW_OBJS) $(HEXEN_OBJS)

# Targets
.PHONY: all clean test triangulate_test bsp_test ui_framework

all: ui_framework doom-ed

# UI Framework library
ui_framework: $(UI_FRAMEWORK_LIB)

$(UI_FRAMEWORK_LIB): $(UI_FRAMEWORK_OBJS)
	@mkdir -p $(dir $@)
	ar rcs $@ $(UI_FRAMEWORK_OBJS)
	@echo "Built UI Framework library: $@"

# Main executable - now depends on UI framework
doom-ed: $(OBJS) $(UI_FRAMEWORK_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) $(UI_FRAMEWORK_LIB) $(LDFLAGS) -o $@

# Object file rules
$(BUILD_DIR)/ui_framework/%.o: $(UI_FRAMEWORK_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(UI_FRAMEWORK_DIR) -c $< -o $@

$(BUILD_DIR)/mapview/%.o: $(MAPVIEW_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(MAPVIEW_DIR) -I$(DOOM_DIR) -I$(HEXEN_DIR) -I$(UI_FRAMEWORK_DIR) -c $< -o $@

$(BUILD_DIR)/mapview/windows/%.o: $(MAPVIEW_DIR)/windows/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(MAPVIEW_DIR) -I$(DOOM_DIR) -I$(HEXEN_DIR) -I$(UI_FRAMEWORK_DIR) -c $< -o $@

$(BUILD_DIR)/mapview/windows/inspector/%.o: $(MAPVIEW_DIR)/windows/inspector/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(MAPVIEW_DIR) -I$(DOOM_DIR) -I$(HEXEN_DIR) -I$(UI_FRAMEWORK_DIR) -c $< -o $@

$(BUILD_DIR)/hexen/%.o: $(HEXEN_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(MAPVIEW_DIR) -I$(DOOM_DIR) -I$(HEXEN_DIR) -c $< -o $@

# Test targets
test: triangulate_test bsp_test
	@echo "=== Running all tests ==="
	@./triangulate_test
	@./bsp_test

triangulate_test: $(TESTS_DIR)/triangulate_test.c $(MAPVIEW_DIR)/triangulate.c
	$(CC) -DTEST_MODE -o $@ $^ -I$(MAPVIEW_DIR) -I$(TESTS_DIR) -lm

bsp_test: $(TESTS_DIR)/bsp_test.c
	$(CC) -o $@ $^ -I$(MAPVIEW_DIR) -I$(TESTS_DIR) -lm

# Clean
clean:
	rm -rf $(BUILD_DIR) triangulate_test bsp_test doom-ed
	rm -f $(MAPVIEW_DIR)/*.o $(MAPVIEW_DIR)/windows/*.o $(MAPVIEW_DIR)/windows/inspector/*.o
	rm -f $(HEXEN_DIR)/*.o $(DOOM_DIR)/*.o $(UI_FRAMEWORK_DIR)/*.o
