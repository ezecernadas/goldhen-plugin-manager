#ifndef __PLGM_MENU_GUI_H__
#define __PLGM_MENU_GUI_H__

#include <stdbool.h>
#include "types.h"

// menu_targets.c
void doTargetsMenu(void);
void Draw_TargetsMenu(u8 alpha);
void Draw_TargetsMenu_Ani(void);

// menu_plugins.c
void doPluginsMenu(void);
void Draw_PluginsMenu(u8 alpha);
void Draw_PluginsMenu_Ani(void);
bool plugins_cursor_is_gamepad_helper(void);

// menu_gamepad.c
void doGamepadMenu(void);
void Draw_GamepadMenu(u8 alpha);
void Draw_GamepadMenu_Ani(void);

#endif
