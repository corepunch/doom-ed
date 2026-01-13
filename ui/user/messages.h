#ifndef __UI_MESSAGES_H__
#define __UI_MESSAGES_H__

// Window messages
enum {
  WM_CREATE,
  WM_DESTROY,
  WM_SHOWWINDOW,
  WM_NCPAINT,
  WM_NCLBUTTONUP,
  WM_PAINT,
  WM_REFRESHSTENCIL,
  WM_PAINTSTENCIL,
  WM_SETFOCUS,
  WM_KILLFOCUS,
  WM_HITTEST,
  WM_COMMAND,
  WM_TEXTINPUT,
  WM_WHEEL,
  WM_MOUSEMOVE,
  WM_MOUSELEAVE,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_RBUTTONDOWN,
  WM_RBUTTONUP,
  WM_RESIZE,
  WM_KEYDOWN,
  WM_KEYUP,
  WM_JOYBUTTONDOWN,
  WM_JOYBUTTONUP,
  WM_JOYAXISMOTION,
  WM_USER = 1000
};

// Control messages
enum {
  BM_SETCHECK = WM_USER,
  BM_GETCHECK,
  CB_ADDSTRING,
  CB_GETCURSEL,
  CB_SETCURSEL,
  CB_GETLBTEXT,
  ST_ADDWINDOW,
  TB_ADDBUTTONS,
  TB_BUTTONCLICK,
};

// Control notification messages
enum {
  EN_UPDATE = 100,
  BN_CLICKED,
  CBN_SELCHANGE,
};

// Button state
enum {
  BST_UNCHECKED,
  BST_CHECKED
};

// Error codes
#define CB_ERR -1

// Window flags
#define WINDOW_NOTITLE      (1 << 0)
#define WINDOW_TRANSPARENT  (1 << 1)
#define WINDOW_VSCROLL      (1 << 2)
#define WINDOW_HSCROLL      (1 << 3)
#define WINDOW_NORESIZE     (1 << 4)
#define WINDOW_NOFILL       (1 << 5)
#define WINDOW_ALWAYSONTOP  (1 << 6)
#define WINDOW_ALWAYSINBACK (1 << 7)
#define WINDOW_HIDDEN       (1 << 8)
#define WINDOW_NOTRAYBUTTON (1 << 9)
#define WINDOW_DIALOG       (1 << 10)
#define WINDOW_TOOLBAR      (1 << 11)

// Titlebar and toolbar dimensions
#define TITLEBAR_HEIGHT   16
#define TOOLBAR_HEIGHT    24
#define RESIZE_HANDLE     8
#define BUTTON_HEIGHT     13

// Control button dimensions
#define CONTROL_BUTTON_WIDTH    8
#define CONTROL_BUTTON_PADDING  2
#define TB_SPACING              18

// Scroll and interaction constants
#define SCROLL_SENSITIVITY      5

// Icon enumerations for UI controls
typedef enum {
  icon8_minus,
  icon8_collapse,
  icon8_maximize,
  icon8_dropdown,
  icon8_checkbox,
  icon8_editor_helmet,
  icon8_count,
} icon8_t;

// UI colors (from sprites.h COLOR_ constants - will be moved to ui framework)
#define COLOR_LIGHT_EDGE     0xff545454
#define COLOR_DARK_EDGE      0xff1a1a1a
#define COLOR_PANEL_BG       0xff3c3c3c
#define COLOR_PANEL_DARK_BG  0xff2c2c2c
#define COLOR_FOCUSED        0xff5EC4F3
#define COLOR_FLARE          0xffffffff
#define COLOR_TEXT_NORMAL    0xffc0c0c0
#define COLOR_TEXT_DISABLED  0xff808080
#define COLOR_TEXT_ERROR     0xffff4444
#define COLOR_TEXT_SUCCESS   0xff44ff44

// Macros for creating rectangles
#define MAKERECT(X, Y, W, H) (&(rect_t){X, Y, W, H})

// Macros for extracting DWORD parts
#define LOWORD(l) ((uint16_t)(l & 0xFFFF))
#define HIWORD(l) ((uint16_t)((l >> 16) & 0xFFFF))
#define MAKEDWORD(low, high) ((uint32_t)(((uint16_t)(low)) | ((uint32_t)((uint16_t)(high))) << 16))

#endif
