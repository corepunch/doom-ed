# Adding UI Framework Target to Xcode

This document describes how to add the UI framework as a separate target in the Xcode project.

## Steps to Add UI Framework Target

### 1. Create a New Target

1. Open `mapview.xcodeproj` in Xcode
2. Select the project in the navigator (top-level "mapview")
3. Click the "+" button at the bottom of the targets list
4. Choose "Library" → "Static Library"
5. Name it "ui_framework"
6. Set the language to C
7. Click "Finish"

### 2. Add Source Files to Target

1. In the Project Navigator, expand the `ui_framework` folder
2. Select all `.c` and `.h` files in `ui_framework/`:
   - `ui_framework.h`
   - `ui_window.c`
   - `ui_sdl.c`
3. Open the File Inspector (⌘⌥1)
4. In "Target Membership", check the "ui_framework" target

### 3. Configure Build Settings

1. Select the "ui_framework" target
2. Go to "Build Settings"
3. Set the following:
   - **Header Search Paths**: Add `$(SRCROOT)/ui_framework`
   - **Other C Flags**: Add `-std=gnu17`
   - **Product Name**: `libui_framework`

### 4. Configure Framework Search Paths for SDL

1. In "Build Settings" for ui_framework target
2. Find "Framework Search Paths"
3. Add SDL2 framework path (typically `/Library/Frameworks` or use Homebrew path)
4. Or use pkg-config:
   - Add `$(shell pkg-config --cflags sdl2)` to "Other C Flags"

### 5. Link mapview Against UI Framework

1. Select the "mapview" target
2. Go to "Build Phases"
3. Expand "Link Binary With Libraries"
4. Click "+" button
5. Select "libui_framework.a" from the list
6. Or manually add it if not listed

### 6. Update Header Search Paths for mapview

1. Select "mapview" target
2. Go to "Build Settings"
3. Find "Header Search Paths"
4. Add `$(SRCROOT)/ui_framework`

### 7. Set Build Order

1. Select "mapview" target
2. Go to "Build Phases"
3. Expand "Target Dependencies"
4. Click "+" button
5. Add "ui_framework" target
6. This ensures UI framework builds before mapview

## Verification

After completing these steps:

1. Build the ui_framework target (⌘B with ui_framework selected)
   - Should produce `libui_framework.a`
   
2. Build the mapview target
   - Should link against libui_framework.a
   - Should compile without errors

## Alternative: Using the Makefile

If you prefer not to use Xcode, the Makefile already supports building the UI framework:

```bash
# Build UI framework only
make ui_framework

# Build everything (framework + mapview)
make all

# Clean everything
make clean
```

## Troubleshooting

### "SDL2/SDL.h not found" Error

- Ensure SDL2 is installed via Homebrew: `brew install sdl2`
- Check that Framework Search Paths includes SDL2 location
- Verify pkg-config can find SDL2: `pkg-config --cflags sdl2`

### "Cannot find libui_framework.a" Error

- Ensure ui_framework target builds successfully first
- Check that it's listed in "Link Binary With Libraries"
- Verify Target Dependencies includes ui_framework

### Header Not Found Errors

- Verify Header Search Paths includes `$(SRCROOT)/ui_framework`
- Check that ui_framework.h is in the ui_framework folder
- Ensure files are added to correct target membership

## Notes

- The UI framework is built as a **static library** (.a file)
- Both ui_framework and mapview link against SDL2
- The framework provides window management; rendering stays in mapview
- See `FRAMEWORK_ARCHITECTURE.md` for design rationale
