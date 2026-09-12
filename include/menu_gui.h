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
/* Config screen for the plugin under the cursor (MENU_* id), or -1. */
int plugins_cursor_config_menu(void);
const char* plugins_cursor_path(void);

// menu_gamepad.c
void doGamepadMenu(void);
void Draw_GamepadMenu(u8 alpha);
void Draw_GamepadMenu_Ani(void);

// menu_language.c
void doLanguageMenu(void);
void Draw_LanguageMenu(u8 alpha);
void Draw_LanguageMenu_Ani(void);
/* Row matching the current override of the selected target, or 0. */
int  language_menu_initial_row(void);

#endif
