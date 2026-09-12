/*
 * Screen dispatch and shared list-drawing helpers.
 */
#include <string.h>

#include "app.h"
#include "orbisPad.h"
#include "menu.h"
#include "menu_gui.h"
#include "libfont.h"

int menu_id = MENU_TARGETS;                     // Menu currently in
int menu_sel = 0;                               // Index of selected item
int menu_old_sel[TOTAL_MENU_IDS] = { 0 };       // Previous menu_sel for each menu

void SetMenu(int id)
{
	switch (id)
	{
		case MENU_TARGETS:
			Draw_TargetsMenu_Ani();
			break;

		case MENU_PLUGINS:
			/* keep the cursor when coming back from the gamepad screen */
			if (menu_id == MENU_TARGETS)
				menu_old_sel[MENU_PLUGINS] = 0;
			Draw_PluginsMenu_Ani();
			break;

		case MENU_GAMEPAD:
			menu_old_sel[MENU_GAMEPAD] = 0;
			Draw_GamepadMenu_Ani();
			break;
	}

	menu_old_sel[menu_id] = menu_sel;
	menu_id = id;
	menu_sel = menu_old_sel[menu_id];
}

void move_selection_back(int count, int steps)
{
	menu_sel -= steps;
	if ((menu_sel == -1) && (steps == 1))
		menu_sel = count - 1;
	else if (menu_sel < 0)
		menu_sel = 0;
}

void move_selection_fwd(int count, int steps)
{
	menu_sel += steps;
	if ((menu_sel == count) && (steps == 1))
		menu_sel = 0;
	else if (menu_sel >= count)
		menu_sel = count - 1;
}

void DrawScrollBar(int selIndex, int max, int maxPerPage, int xOff, u8 alpha)
{
	int y = help_png_y;
	int yTotal = help_png_h;
	int cursor_h;
	float pLoc;

	if (max <= maxPerPage || max <= 1)
		return;

	if (orbisPadGetConf()->idle > 80)
	{
		int dec = (orbisPadGetConf()->idle - 80) * 4;
		if (dec > alpha)
			dec = alpha;
		alpha -= dec;
	}

	cursor_h = (yTotal * maxPerPage) / max;
	if (cursor_h < 24)
		cursor_h = 24;

	pLoc = (float)selIndex / (float)(max - 1);

	SetFontAlign(FONT_ALIGN_LEFT);

	//Draw box
	DrawTextureCenteredX(&menu_textures[scroll_bg_png_index], xOff, y, 0, menu_textures[scroll_bg_png_index].width, yTotal, 0xffffff00 | alpha);

	//Draw cursor
	DrawTextureCenteredX(&menu_textures[scroll_lock_png_index], xOff, y + (int)((float)(yTotal - cursor_h) * pLoc), 0, menu_textures[scroll_lock_png_index].width, cursor_h, 0xffffff00 | alpha);
}

void DrawSelectionMark(int y, int y_inc, int arrow_x, u8 alpha)
{
	DrawTexture(&menu_textures[mark_line_png_index], 0, y, 0, SCREEN_WIDTH, y_inc, 0xFFFFFF00 | alpha);
	DrawTextureCenteredX(&menu_textures[mark_arrow_png_index], arrow_x, y + (y_inc - (APP_LINE_OFFSET + 2)) / 2, 0, (2 * APP_LINE_OFFSET) / 3, APP_LINE_OFFSET + 2, 0xFFFFFF00 | alpha);
}

void drawScene(void)
{
	switch (menu_id)
	{
		case MENU_TARGETS:
			doTargetsMenu();
			break;

		case MENU_PLUGINS:
			doPluginsMenu();
			break;

		case MENU_GAMEPAD:
			doGamepadMenu();
			break;
	}
}
