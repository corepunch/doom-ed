# UI Framework

This directory contains the extracted UI framework for DOOM-ED, organized in a Windows-like architecture.

## Directory Structure

```
ui/
├── ui.h              # Main header that includes all UI subsystems
├── user/             # Window management and user interface (USER.DLL equivalent)
│   ├── user.h        # Window structures and management functions
│   ├── messages.h    # Window message constants and macros
│   └── draw.h        # Drawing primitives (rectangles, text, icons)
├── kernel/           # Event loop and SDL integration (KERNEL.DLL equivalent)
│   └── kernel.h      # Event management and SDL initialization
└── commctl/          # Common controls (COMCTL32.DLL equivalent)
    ├── commctl.h     # Common control window procedures
    ├── button.c      # Button control implementation
    ├── checkbox.c    # Checkbox control implementation
    ├── edit.c        # Text edit control implementation
    ├── label.c       # Label (static text) control implementation
    ├── list.c        # List control implementation
    └── combobox.c    # Combobox (dropdown) control implementation
```

## Architecture

The UI framework follows a layered architecture similar to Windows:

### ui/user/ - Window Management Layer
Handles window creation, destruction, message passing, and basic rendering primitives.

**Key Components:**
- Window structure (`window_t`)
- Window creation and lifecycle management
- Message queue and dispatch
- Drawing primitives (rectangles, text, icons)
- Window messages (WM_CREATE, WM_PAINT, WM_LBUTTONUP, etc.)

### ui/kernel/ - Event Management Layer
Manages the SDL event loop and translates SDL events into window messages.

**Key Components:**
- SDL initialization
- Event loop (`get_message`, `dispatch_message`)
- Global state (screen dimensions, running flag)

### ui/commctl/ - Common Controls Layer
Implements standard UI controls that can be used to build interfaces.

**Available Controls:**
- **Button**: Clickable button with text label
- **Checkbox**: Toggle control with checkmark
- **Edit**: Single-line text input control
- **Label**: Static text display
- **List**: Scrollable list of items
- **Combobox**: Dropdown selection control

## Usage

Include the main header in your code:

```c
#include "ui/ui.h"
```

Or include specific subsystems:

```c
#include "ui/user/user.h"
#include "ui/commctl/commctl.h"
```

### Creating a Window

```c
rect_t frame = {100, 100, 200, 150};
window_t *win = create_window("My Window", 0, &frame, NULL, my_window_proc, NULL);
show_window(win, true);
```

### Creating Controls

```c
// Create a button
window_t *btn = create_window("Click Me", 0, &btn_frame, parent, win_button, NULL);

// Create a checkbox
window_t *chk = create_window("Enable Feature", 0, &chk_frame, parent, win_checkbox, NULL);

// Create an edit box
window_t *edit = create_window("Enter text", 0, &edit_frame, parent, win_textedit, NULL);
```

## Window Messages

The framework uses a message-based architecture. Common messages include:

- `WM_CREATE` - Window is being created
- `WM_DESTROY` - Window is being destroyed
- `WM_PAINT` - Window needs to be redrawn
- `WM_LBUTTONDOWN` - Left mouse button pressed
- `WM_LBUTTONUP` - Left mouse button released
- `WM_KEYDOWN` - Key pressed
- `WM_KEYUP` - Key released
- `WM_COMMAND` - Control notification

## Control-Specific Messages

### Button Messages
- `BN_CLICKED` - Button was clicked

### Checkbox Messages
- `BM_SETCHECK` - Set checkbox state
- `BM_GETCHECK` - Get checkbox state

### Combobox Messages
- `CB_ADDSTRING` - Add item to combobox
- `CB_GETCURSEL` - Get currently selected item
- `CB_SETCURSEL` - Set currently selected item
- `CBN_SELCHANGE` - Selection changed notification

### Edit Box Messages
- `EN_UPDATE` - Text was modified

## Status

This is an in-progress refactoring. The framework currently:

✅ Has header files defining the API structure
✅ Has extracted common control implementations (button, checkbox, edit, label, list, combobox)
⏳ Still needs core window management code to be moved from mapview/window.c
⏳ Still needs drawing primitives to be moved from mapview/sprites.c
⏳ Still needs text rendering to be moved from mapview/windows/console.c
⏳ Needs build system integration

## Future Work

1. Move core window management functions to `ui/user/window.c`
2. Move message queue to `ui/user/message.c`
3. Move drawing primitives to `ui/user/draw.c`
4. Move text rendering to `ui/user/text.c`
5. Move font system to `ui/user/font.c`
6. Update build system (Makefile, Xcode project)
7. Update all #includes throughout the codebase
8. Add documentation for each function
9. Add example code
10. Add unit tests for UI components
