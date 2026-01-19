# Building DOOM-ED with Make

This document describes how to build DOOM-ED using the provided Makefile on Linux and macOS.

## Prerequisites

### Clone with Submodules

When cloning the repository, make sure to initialize the submodules to get the WAD files:

```bash
git clone --recurse-submodules https://github.com/corepunch/doom-ed.git
```

Or if you already cloned the repository:

```bash
git submodule update --init --recursive
```

### Linux (Ubuntu/Debian)
```bash
sudo apt-get install build-essential libsdl2-dev libcglm-dev liblua5.4-dev
```

### macOS
```bash
brew install sdl2 cglm lua
```

## Building

### Build the main executable
```bash
make
```
or
```bash
make all
```

This will create the `doom-ed` executable in the current directory.

### Build and run tests
```bash
make test
```

This will compile and run both the triangulation tests and BSP tests.

### Build individual test binaries
```bash
make triangulate_test
make bsp_test
```

### Clean build artifacts
```bash
make clean
```

This removes all object files, executables, and the build directory.

## Running

The main executable requires a WAD file argument:

```bash
./doom-ed path/to/yourfile.wad
```

## Platform Support

The Makefile automatically detects the platform and uses appropriate settings:

- **macOS**: Uses OpenGL framework and Homebrew paths for dependencies
- **Linux**: Uses pkg-config to locate SDL2 and cglm, links against libGL

### OpenGL Headers

The code uses a compatibility header (`mapview/gl_compat.h`) that automatically includes the correct OpenGL headers for each platform:
- macOS: `<OpenGL/gl3.h>`
- Linux: `<GL/glcorearb.h>` and `<GL/gl.h>`

## Build Output

- **Executable**: `doom-ed` (main application)
- **Test binaries**: `triangulate_test`, `bsp_test`
- **Object files**: `build/` directory
- **Build directory**: `build/` (contains intermediate .o files)

## Troubleshooting

### SDL2 not found
If pkg-config cannot find SDL2, you may need to set PKG_CONFIG_PATH:
```bash
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
```

### OpenGL headers not found
Make sure you have the OpenGL development headers installed:
- Linux: `sudo apt-get install libgl1-mesa-dev`
- macOS: OpenGL is included with Xcode Command Line Tools

### cglm not found
Install cglm library:
- Linux: `sudo apt-get install libcglm-dev`
- macOS: `brew install cglm`

### Lua not found
Install Lua 5.4 library:
- Linux: `sudo apt-get install liblua5.4-dev`
- macOS: `brew install lua`

## Compiler Flags

The Makefile uses the following compiler flags:
- `-Wall`: Enable all warnings
- `-std=gnu17`: Use GNU C17 standard
- `-DGL_SILENCE_DEPRECATION`: Silence OpenGL deprecation warnings
- `-lm`: Link math library
- `-lGL` (Linux) or `-framework OpenGL` (macOS): Link OpenGL
- `-lSDL2`: Link SDL2
- `-lcglm` (or via pkg-config): Link cglm

## Dependencies

- **SDL2**: Window management and input handling
- **OpenGL**: 3D graphics rendering
- **cglm**: OpenGL Mathematics (GLM) for C
- **Lua 5.4**: Scripting support for the UI terminal
- **Math library**: Standard C math functions

## Notes

- The executable is named `doom-ed` to avoid conflicts with the `mapview` source directory
- All tests from the original project continue to pass
- The build system supports both macOS and Linux platforms
