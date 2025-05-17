# DOOM-ED

![DOOM-ED Screenshot](https://github.com/corepunch/doom-ed/raw/master/screenshots/editor.png)

A modern open-source level editor for classic DOOM games, built with SDL2.

## Features

- Full-featured level editor for DOOM, DOOM II, Heretic, and other DOOM engine games
- Support for PWAD/IWAD file formats
- Real-time 2D and 3D preview
- Vertex manipulation tools
- Sector creation and editing
- Texture browser and management
- Thing placement and properties editor
- Linedef and sidedef editing
- Scriptable with Lua for custom tools and automation
- Cross-platform support (Windows, macOS, Linux)

## Requirements

- SDL2
- OpenGL
- C++11 compatible compiler
- CMake (for building)

## Building

### Windows

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### macOS and Linux

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

### Basic Controls

- **Left Mouse**: Select objects
- **Right Mouse**: Context menu/Pan view
- **Middle Mouse**: Drag to pan view
- **Scroll Wheel**: Zoom in/out
- **Shift + Left Mouse**: Add to selection
- **Ctrl + Left Mouse**: Remove from selection
- **Shift + Drag**: Create sector
- **Tab**: Switch between 2D and 3D view

### Keyboard Shortcuts

- **Ctrl+N**: New map
- **Ctrl+O**: Open WAD file
- **Ctrl+S**: Save
- **Ctrl+Z**: Undo
- **Ctrl+Y**: Redo
- **Delete**: Delete selected objects
- **G**: Grid toggle
- **F**: Focus on selection
- **V**: Vertex mode
- **L**: Line mode
- **S**: Sector mode
- **T**: Thing mode
- **Esc**: Cancel current operation

## File Structure

- `src/`: Source code
- `include/`: Header files
- `resources/`: Icons, shaders, and other resources
- `lua/`: Lua scripts for editor extension
- `docs/`: Documentation

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
