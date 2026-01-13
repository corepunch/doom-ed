//
//  commctl.h
//  Common Controls - Standard UI controls library
//
//  Like Windows Common Controls (ComCtl32)
//

#ifndef __COMMCTL_H__
#define __COMMCTL_H__

#include "../ui_framework.h"

// Common control window procedures
// These are generic UI controls that can be used in any application

// Button control
result_t win_button(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

// Checkbox control
result_t win_checkbox(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

// Text edit control (single-line)
result_t win_textedit(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

// Label (static text) control
result_t win_label(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

// List control
result_t win_list(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

// Combobox control (dropdown list)
result_t win_combobox(window_t *win, uint32_t msg, uint32_t wparam, void *lparam);

// Control notification codes
#define BN_CLICKED    1  // Button clicked
#define EN_UPDATE     1  // Edit control updated
#define CBN_SELCHANGE 1  // Combobox selection changed

// List control messages
#define LIST_SELITEM  0x5001

#endif // __COMMCTL_H__
