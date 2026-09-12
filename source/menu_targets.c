/*
 * Targets screen: "Global" followed by every installed game/app.
 */
#include <string.h>
#include <stdlib.h>

#include "app.h"
#include "orbisPad.h"
#include "menu.h"
#include "menu_gui.h"
#include "libfont.h"
#include "common.h"
#include "icon_cache.h"

#define TARGET_ROW_HEIGHT       72
#define TARGET_ICON_SIZE        64
#define TARGET_LIST_Y           (help_png_y + 20)
#define TARGET_LIST_H           (help_png_h - 40)
#define TARGET_MAX_PER_PAGE     (TARGET_LIST_H / TARGET_ROW_HEIGHT)
#define TARGET_NAME_X           (MENU_ICON_OFF + TARGET_ICON_SIZE + 30)
#define TARGET_ID_X             (SCREEN_WIDTH - 180)

static int target_count(void)
{
	return 1 + (int)list_count(app.installed);
}

static const app_entry_t* target_at(int index)
{
	if (index <= 0)
		return NULL;

	return list_get_item(app.installed, index - 1);
}

static void draw_row(int index, int y, u8 alpha)
{
	const app_entry_t* entry = target_at(index);
	SDL_Texture* icon;
	char* name;
	int max_width = TARGET_ID_X - 260 - TARGET_NAME_X;

	if (index == 0)
	{
		DrawTexture(&menu_textures[titlescr_ico_opt_png_index], MENU_ICON_OFF, y + 4, 0, TARGET_ICON_SIZE, TARGET_ICON_SIZE, 0xFFFFFF00 | alpha);
		SetFontColor(APP_FONT_COLOR | alpha, 0);
		DrawString(TARGET_NAME_X, y + 12, "Global");
		SetFontColor(APP_FONT_HINT_COLOR | alpha, 0);
		SetFontAlign(FONT_ALIGN_RIGHT);
		DrawString(TARGET_ID_X, y + 12, "[default]");
		SetFontAlign(FONT_ALIGN_LEFT);
		return;
	}

	icon = icon_cache_get(entry->title_id);
	DrawRawTexture(icon, MENU_ICON_OFF, y + 4, TARGET_ICON_SIZE, TARGET_ICON_SIZE, alpha);

	SetFontColor(APP_FONT_COLOR | alpha, 0);
	name = strdup(entry->name ? entry->name : entry->title_id);
	if (name)
	{
		while (name[0] && WidthFromStr(name) > max_width)
			utf8_truncate(name, strlen(name) - 1);
		DrawString(TARGET_NAME_X, y + 12, name);
		free(name);
	}

	SetFontColor(APP_FONT_HINT_COLOR | alpha, 0);
	SetFontAlign(FONT_ALIGN_RIGHT);
	DrawString(TARGET_ID_X, y + 12, entry->title_id);
	SetFontAlign(FONT_ALIGN_LEFT);
}

static void draw_list(int selIndex, u8 alpha)
{
	int count = target_count();
	int maxPerPage = TARGET_MAX_PER_PAGE;
	int first = selIndex - (maxPerPage / 2);
	int y = TARGET_LIST_Y;
	int i;

	if (first > count - maxPerPage)
		first = count - maxPerPage;
	if (first < 0)
		first = 0;

	SetFontSize(APP_FONT_SIZE_SELECTION);

	for (i = first; i < first + maxPerPage && i < count; i++)
	{
		if (i == selIndex)
			DrawSelectionMark(y, TARGET_ROW_HEIGHT, MENU_ICON_OFF - 40, alpha);

		draw_row(i, y, alpha);

		y += TARGET_ROW_HEIGHT;
	}

	DrawScrollBar(selIndex, count, maxPerPage, SCREEN_WIDTH - 125, alpha);
}

void Draw_TargetsMenu(u8 alpha)
{
	DrawHeader(header_ico_xmb_png_index, 0, "Plugin Manager", "Select a target", APP_FONT_TITLE_COLOR | alpha, 0xffffffff, 0);
	draw_list(menu_sel, alpha);
}

void Draw_TargetsMenu_Ani(void)
{
	int ani;

	for (ani = 0; ani < MENU_ANI_MAX; ani++)
	{
		SDL_RenderClear(renderer);
		DrawHeader_Ani(header_ico_xmb_png_index, "Plugin Manager", "Select a target", APP_FONT_TITLE_COLOR, 0xffffffff, ani, 12);

		u8 icon_a = (u8)(((ani * 2) > 0xFF) ? 0xFF : (ani * 2));
		int _game_a = (int)(icon_a - (MENU_ANI_MAX / 2)) * 2;
		if (_game_a > 0xFF)
			_game_a = 0xFF;
		u8 game_a = (u8)(_game_a < 0 ? 0 : _game_a);

		if (game_a > 0)
			draw_list(menu_old_sel[MENU_TARGETS], game_a);

		SDL_RenderPresent(renderer);

		if (game_a == 0xFF)
			return;
	}
}

static void select_target(int index)
{
	const app_entry_t* entry = target_at(index);

	if (!entry)
	{
		strncpy(app.target_id, PM_TARGET_DEFAULT, sizeof(app.target_id) - 1);
		strncpy(app.target_name, "Global", sizeof(app.target_name) - 1);
	}
	else
	{
		strncpy(app.target_id, entry->title_id, sizeof(app.target_id) - 1);
		strncpy(app.target_name, entry->name ? entry->name : entry->title_id, sizeof(app.target_name) - 1);
	}
	app.target_id[sizeof(app.target_id) - 1] = '\0';
	app.target_name[sizeof(app.target_name) - 1] = '\0';
	utf8_truncate(app.target_name, sizeof(app.target_name) - 1);
	app.dirty = false;
}

void doTargetsMenu(void)
{
	int count = target_count();

	if (orbisPadGetButtonHold(ORBIS_PAD_BUTTON_UP))
		move_selection_back(count, 1);

	else if (orbisPadGetButtonHold(ORBIS_PAD_BUTTON_DOWN))
		move_selection_fwd(count, 1);

	else if (orbisPadGetButtonHold(ORBIS_PAD_BUTTON_LEFT))
		move_selection_back(count, 5);

	else if (orbisPadGetButtonHold(ORBIS_PAD_BUTTON_L1))
		move_selection_back(count, 25);

	else if (orbisPadGetButtonHold(ORBIS_PAD_BUTTON_L2))
		menu_sel = 0;

	else if (orbisPadGetButtonHold(ORBIS_PAD_BUTTON_RIGHT))
		move_selection_fwd(count, 5);

	else if (orbisPadGetButtonHold(ORBIS_PAD_BUTTON_R1))
		move_selection_fwd(count, 25);

	else if (orbisPadGetButtonHold(ORBIS_PAD_BUTTON_R2))
		menu_sel = count - 1;

	else if (orbisPadGetButtonPressed(ORBIS_PAD_BUTTON_CROSS))
	{
		select_target(menu_sel);
		SetMenu(MENU_PLUGINS);
		return;
	}
	else if (orbisPadGetButtonPressed(ORBIS_PAD_BUTTON_CIRCLE))
	{
		int exit_app = show_dialog(1, "Exit to XMB?");

		// Consume the button press that dismissed the dialog
		orbisPadUpdate();
		if (exit_app)
		{
			close_app = 1;
			return;
		}
	}

	Draw_TargetsMenu(0xFF);
}
