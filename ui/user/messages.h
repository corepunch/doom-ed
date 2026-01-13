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
#define WINDOW_NOTITLE    (1 << 0)
#define WINDOW_NORESIZE   (1 << 1)
#define WINDOW_NOFILL     (1 << 2)
#define WINDOW_TOOLBAR    (1 << 3)

// Titlebar and toolbar dimensions
#define TITLEBAR_HEIGHT   16
#define TOOLBAR_HEIGHT    24
#define RESIZE_HANDLE     8

// UI colors (from COLOR_ constants)
#define COLOR_LIGHT_EDGE     0xFFCCCCCC
#define COLOR_DARK_EDGE      0xFF444444
#define COLOR_PANEL_BG       0xFF888888
#define COLOR_PANEL_DARK_BG  0xFF666666
#define COLOR_FOCUSED        0xFFFF8800
#define COLOR_FLARE          0xFFFFFFFF

// Macros for creating rectangles
#define MAKERECT(X, Y, W, H) (&(rect_t){X, Y, W, H})

// Macros for extracting DWORD parts
#define LOWORD(l) ((uint16_t)(l & 0xFFFF))
#define HIWORD(l) ((uint16_t)((l >> 16) & 0xFFFF))
#define MAKEDWORD(low, high) ((uint32_t)(((uint16_t)(low)) | ((uint32_t)((uint16_t)(high))) << 16))

#endif
