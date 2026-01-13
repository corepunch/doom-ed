# UI Framework Migration Guide

This document explains the current state of the UI framework extraction and how to complete the integration.

## What Has Been Done

### 1. Framework Structure Created
A complete directory structure has been set up under `ui/` following a Windows-like architecture:

```
ui/
├── ui.h              # Master include file
├── README.md         # Framework documentation
├── user/             # Window management layer
│   ├── user.h        # Window API
│   ├── messages.h    # Message constants
│   └── draw.h        # Drawing API
├── kernel/           # Event management layer
│   └── kernel.h      # SDL event loop API
└── commctl/          # Common controls layer
    ├── commctl.h     # Controls API
    ├── button.c      # ✅ Extracted
    ├── checkbox.c    # ✅ Extracted
    ├── edit.c        # ✅ Extracted
    ├── label.c       # ✅ Extracted
    ├── list.c        # ✅ Extracted
    └── combobox.c    # ✅ Extracted
```

### 2. Common Controls Extracted
All 6 common control implementations have been extracted from `mapview/window.c`:
- Button control (`win_button`)
- Checkbox control (`win_checkbox`)
- Text edit control (`win_textedit`)
- Label control (`win_label`)
- List control (`win_list`)
- Combobox control (`win_combobox`)

These controls are now in separate `.c` files under `ui/commctl/` and can be compiled independently.

### 3. API Definitions Created
Complete header files define the interfaces for:
- Window management (creation, destruction, messaging)
- Drawing primitives (rectangles, text, icons)
- Event handling (SDL integration)
- Common controls

## What Still Needs to Be Done

### Phase 1: Integrate UI Controls into Build (NEXT STEP)

**Goal:** Get the extracted controls compiling as part of the build.

**Tasks:**
1. Update `Makefile` to include UI framework sources
2. Test compilation with existing code
3. Ensure no duplicate symbols

**Makefile Changes Needed:**
```makefile
# Add UI directory
UI_DIR = ui

# Add UI source files
UI_SRCS = $(UI_DIR)/commctl/button.c \
          $(UI_DIR)/commctl/checkbox.c \
          $(UI_DIR)/commctl/edit.c \
          $(UI_DIR)/commctl/label.c \
          $(UI_DIR)/commctl/list.c \
          $(UI_DIR)/commctl/combobox.c

# Add UI object files
UI_OBJS = $(UI_SRCS:$(UI_DIR)/%.c=$(BUILD_DIR)/ui/%.o)

# Update OBJS to include UI_OBJS
OBJS = $(MAPVIEW_OBJS) $(HEXEN_OBJS) $(UI_OBJS)

# Add build rule for UI objects
$(BUILD_DIR)/ui/%.o: $(UI_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(UI_DIR) -I$(MAPVIEW_DIR) -I$(DOOM_DIR) -I$(HEXEN_DIR) -c $< -o $@
```

### Phase 2: Update mapview/window.c

**Goal:** Remove duplicated control implementations from `mapview/window.c` and reference the UI framework versions instead.

**Tasks:**
1. Remove `win_button`, `win_checkbox`, etc. implementations from `mapview/window.c`
2. Add `#include "ui/commctl/commctl.h"` to `mapview/window.c`
3. Verify that controls still work

**Before:**
```c
// In mapview/window.c
result_t win_button(window_t *win, uint32_t msg, uint32_t wparam, void *lparam) {
  // ... implementation ...
}
```

**After:**
```c
// In mapview/window.c - controls are now in ui/commctl/
#include "../ui/commctl/commctl.h"

// win_button, win_checkbox, etc. are now provided by ui/commctl/
```

### Phase 3: Extract Core Window Management

**Goal:** Move window management functions to `ui/user/window.c`

**Tasks:**
1. Create `ui/user/window.c` 
2. Move these functions from `mapview/window.c`:
   - `create_window`
   - `destroy_window`
   - `show_window`
   - `move_window`
   - `resize_window`
   - `get_root_window`
   - `find_window`
   - `push_window`
   - Window lifecycle helpers
3. Export global state (`windows`, `_focused`, `_tracked`, `_captured`)

### Phase 4: Extract Message Queue

**Goal:** Move message queue to `ui/user/message.c`

**Tasks:**
1. Create `ui/user/message.c`
2. Move these functions from `mapview/window.c`:
   - `send_message`
   - `post_message`
   - `get_message`
   - `dispatch_message`
   - `repost_messages`
3. Move message queue data structure

### Phase 5: Extract Drawing Primitives

**Goal:** Consolidate drawing functions in `ui/user/`

**Tasks:**
1. Create `ui/user/draw.c`
2. Move from `mapview/window.c`:
   - `draw_button`
   - `draw_panel` (static)
   - `draw_focused` (static)
   - `draw_bevel` (static)
   - `set_viewport`
3. Move from `mapview/sprites.c`:
   - `fill_rect`
   - `draw_rect`
   - `draw_rect_ex`
   - `draw_icon8`
   - `draw_icon16`
4. Move from `mapview/windows/console.c`:
   - `draw_text_gl3`
   - `draw_text_small`
   - `strwidth`
   - `strnwidth`

### Phase 6: Extract Text Rendering

**Goal:** Move font and text rendering to `ui/user/`

**Tasks:**
1. Create `ui/user/font.c`
2. Move font management from `mapview/windows/console.c`:
   - Font atlas creation
   - Font loading
   - Character rendering
3. Create `ui/user/text.c`
4. Move text rendering implementation

### Phase 7: Update All Includes

**Goal:** Update include paths throughout the codebase

**Files to Update:**
- All files in `mapview/windows/`
- All files in `mapview/windows/inspector/`
- Any other files using window management

**Before:**
```c
#include "map.h"  // Contains window_t and UI definitions
```

**After:**
```c
#include "ui/ui.h"  // Use UI framework
#include "map.h"    // Now only contains game-specific types
```

### Phase 8: Update Xcode Project

**Goal:** Add UI framework to Xcode project

**Tasks:**
1. Add `ui/` directory to project
2. Add all `.c` and `.h` files
3. Configure include paths
4. Build and test

## Testing Strategy

After each phase:

1. **Compile Test**
   ```bash
   make clean
   make
   ```

2. **Runtime Test**
   ```bash
   ./doom-ed
   ```

3. **UI Interaction Test**
   - Open the editor
   - Click buttons
   - Toggle checkboxes
   - Type in text fields
   - Use dropdown menus
   - Verify inspector windows work

4. **No Regression Test**
   - All existing functionality should still work
   - No crashes or visual glitches
   - Controls should respond normally

## Benefits

Once complete, the UI framework will provide:

1. **Separation of Concerns**
   - UI code separated from game logic
   - Window management separated from rendering
   - Controls separated from application code

2. **Reusability**
   - UI framework can be used in other projects
   - Controls can be easily reused
   - Clear APIs for custom windows

3. **Maintainability**
   - Easier to understand code organization
   - Clear ownership of different layers
   - Better documentation

4. **Testability**
   - UI components can be tested independently
   - Mock window systems for unit tests
   - Easier to add new controls

## Notes

- The framework uses a Windows-like message-based architecture
- Controls are window procedures that respond to messages
- All coordinates are in screen space
- The framework is currently tightly coupled with OpenGL rendering
- Some game-specific types (sprites, textures) remain in mapview for now

## Questions?

See `ui/README.md` for detailed framework documentation.
