# DOOM-ED

![DOOM-ED Screenshot](https://github.com/corepunch/doom-ed/raw/main/screenshots/editor.png)

A modern open-source level editor for classic DOOM games, built with SDL2 and C.

## Features

- Full-featured level editor for DOOM, DOOM II, Heretic, and other DOOM engine games
- Support for PWAD/IWAD file formats
- Real-time 2D map editing with visual feedback
- 3D map preview and navigation
- Vertex manipulation and geometry editing
- Sector creation and editing with triangulation
- BSP tree visualization and traversal
- Texture management and display
- Thing placement and properties
- Linedef and sidedef editing
- Inspector panels for editing map elements
- Built with the Orion UI framework for window management ([orion-ui](https://github.com/corepunch/orion-ui))
- Cross-platform support (macOS, Linux)

## Requirements

- SDL2 (window management and input)
- OpenGL (3D rendering)
- cglm (OpenGL Mathematics library for C)
- C17 compatible compiler (gcc)
- Make (for building with Makefile)

## Building

### Prerequisites

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install build-essential libsdl2-dev libcglm-dev libgl1-mesa-dev
```

**macOS:**
```bash
brew install sdl2 cglm
```

OpenGL is included with Xcode Command Line Tools on macOS.

### Clone the Repository

After cloning, initialize the submodules to get the WAD files and the Orion UI framework ([orion-ui](https://github.com/corepunch/orion-ui)):

```bash
git submodule update --init --recursive
```

### Build with Make

Build the main executable:
```bash
make
```

This will create the `doom-ed` executable in the current directory.

### Clean Build Artifacts

```bash
make clean
```

This removes all object files, executables, and the build directory.

### Troubleshooting

**SDL2 not found:**
If pkg-config cannot find SDL2, you may need to set PKG_CONFIG_PATH:
```bash
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
```

**OpenGL headers not found:**
Make sure you have the OpenGL development headers installed:
- Linux: `sudo apt-get install libgl1-mesa-dev`
- macOS: OpenGL is included with Xcode Command Line Tools

**cglm not found:**
Install cglm library:
- Linux: `sudo apt-get install libcglm-dev`
- macOS: `brew install cglm`

## Usage

The editor requires a WAD file to load:

```bash
./doom-ed path/to/yourfile.wad
```

You can use the WAD files included via git submodules in the `wads/` directory.

### Basic Controls

- **Left Mouse**: Select objects
- **Mouse Wheel**: Zoom in/out
- **WASD / Arrow Keys**: Navigate in map view
- **Tab**: Switch top/first-person views in 3D view
- **Mouse Movement**: Pan/navigate the view

The editor includes multiple inspector windows for editing different map elements (vertices, lines, sectors, things).

## Project Structure

```
/
├── mapview/              # Core map viewer and renderer
│   ├── *.c, *.h         # Map rendering, BSP, textures, sprites
│   └── main.c           # Application entry point
├── editor/              # Editor-specific functionality
│   ├── editor_*.c       # Drawing, input, sector editing
│   ├── radial_menu.*    # Radial menu system
│   └── windows/         # Editor windows (inspector, game view, etc.)
│       └── inspector/   # Property inspector for map elements
├── ui/                  # Orion UI framework - orion-ui module (git submodule)
│   ├── kernel/          # Event handling and SDL integration
│   ├── user/            # Window system API
│   └── commctl/         # Common controls (buttons, lists, etc.)
├── doom/                # DOOM engine headers and some source
├── hexen/               # Hexen game engine headers and source
├── tests/               # Test suite (triangulation, BSP)
├── screenshots/         # Screenshot images
├── wads/                # WAD files (git submodule)
├── Makefile             # Build system
└── mapview.xcodeproj/   # Xcode project files
```

## Testing

Run all tests:
```bash
make test
```

This will run the triangulation tests and BSP tree traversal tests.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the GPL v3 License - see the LICENSE file for details.

## Acknowledgments

- id Software for creating DOOM
- The DOOM modding community for their dedication and tools that inspired this project
