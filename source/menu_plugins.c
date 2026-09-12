/*
 * Plugins screen: one checkbox per .prx for the selected target.
 */
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "app.h"
#include "orbisPad.h"
#include "menu.h"
#include "menu_gui.h"
#include "libfont.h"
#include "storage.h"

#define PLUGIN_ROW_HEIGHT       56
#define PLUGIN_LIST_Y           (help_png_y + 20)
#define PLUGIN_LIST_H           (help_png_h - 40 - 60)
#define PLUGIN_MAX_PER_PAGE     (PLUGIN_LIST_H / PLUGIN_ROW_HEIGHT)
#define PLUGIN_NAME_X           (MENU_ICON_OFF + MENU_TITLE_OFF + 50)
#define PLUGIN_HINT_Y           (help_png_y + help_png_h - 60)

static const char* plugin_at(int index)
{
	return list_get_item(app.plugins, index);
}

/* Plugins that have a configuration screen, keyed by basename. */
static const struct
{
	const char* basename;
	int menu_id;
} CONFIG_SCREENS[] = {
	{ GAMEPAD_HELPER_PRX,  MENU_GAMEPAD },
	{ SYSTEM_LANGUAGE_PRX, MENU_LANGUAGE },
};

static int config_menu_for(const char* path)
{
	const char* base = path ? strrchr(path, '/') : NULL;
	size_t i;

	base = base ? base + 1 : path;
	if (!base)
		return -1;

	for (i = 0; i < sizeof(CONFIG_SCREENS) / sizeof(CONFIG_SCREENS[0]); i++)
		if (strcasecmp(base, CONFIG_SCREENS[i].basename) == 0)
			return CONFIG_SCREENS[i].menu_id;

	return -1;
}

int plugins_cursor_config_menu(void)
{
	return config_menu_for(plugin_at(menu_sel));
}

/* Full path of the plugin under the cursor, so a config screen edits the
 * exact entry the user selected (the on-disk name may differ in case). */
const char* plugins_cursor_path(void)
{
	return plugin_at(menu_sel);
}

static void draw_row(int index, int y, u8 alpha)
{
	const char* path = plugin_at(index);
	char name[128];
	int c;

	pm_plugin_display_name(path, name, sizeof(name));

	SetFontColor(APP_FONT_COLOR | alpha, 0);
	DrawString(PLUGIN_NAME_X, y + 4, name);

	c = pm_model_is_enabled(app.model, app.target_id, path) ? opt_on_png_index : opt_off_png_index;
	DrawTexture(&menu_textures[c], OPTION_ITEM_OFF - 29, y + 4, 0, menu_textures[c].width, menu_textures[c].height, 0xFFFFFF00 | alpha);
}

static void draw_list(int selIndex, u8 alpha)
{
	int count = (int)list_count(app.plugins);
	int maxPerPage = PLUGIN_MAX_PER_PAGE;
	int first = selIndex - (maxPerPage / 2);
	int y = PLUGIN_LIST_Y;
	int i;

	SetFontSize(APP_FONT_SIZE_SELECTION);

	if (count == 0)
	{
		SetFontColor(APP_FONT_COLOR | alpha, 0);
		SetFontAlign(FONT_ALIGN_SCREEN_CENTER);
		DrawString(0, SCREEN_HEIGHT / 2 - 24, "No plugins found in " GOLDHEN_PLUGINS_DIR);
		SetFontAlign(FONT_ALIGN_LEFT);
	}

	if (first > count - maxPerPage)
		first = count - maxPerPage;
	if (first < 0)
		first = 0;

	for (i = first; i < first + maxPerPage && i < count; i++)
	{
		if (i == selIndex)
			DrawSelectionMark(y, PLUGIN_ROW_HEIGHT, MENU_ICON_OFF + MENU_TITLE_OFF, alpha);

		draw_row(i, y, alpha);

		y += PLUGIN_ROW_HEIGHT;
	}

	DrawScrollBar(selIndex, count, maxPerPage, SCREEN_WIDTH - 125, alpha);

	// Persistent hint
	SetFontSize(APP_FONT_SIZE_SUBTEXT);
	SetFontColor(APP_FONT_HINT_COLOR | alpha, 0);
	SetFontAlign(FONT_ALIGN_SCREEN_CENTER);
	DrawString(0, PLUGIN_HINT_Y, "Changes apply when the game is (re)launched");
	SetFontAlign(FONT_ALIGN_LEFT);
}

void Draw_PluginsMenu(u8 alpha)
{
	DrawHeader(header_ico_opt_png_index, 0, app.target_name, app.target_id, APP_FONT_TITLE_COLOR | alpha, 0xffffffff, 0);
	draw_list(menu_sel, alpha);
}

void Draw_PluginsMenu_Ani(void)
{
	int ani;

	for (ani = 0; ani < MENU_ANI_MAX; ani++)
	{
		SDL_RenderClear(renderer);
		DrawHeader_Ani(header_ico_opt_png_index, app.target_name, app.target_id, APP_FONT_TITLE_COLOR, 0xffffffff, ani, 12);

		u8 icon_a = (u8)(((ani * 2) > 0xFF) ? 0xFF : (ani * 2));
		int _game_a = (int)(icon_a - (MENU_ANI_MAX / 2)) * 2;
		if (_game_a > 0xFF)
			_game_a = 0xFF;
		u8 game_a = (u8)(_game_a < 0 ? 0 : _game_a);

		if (game_a > 0)
			draw_list(menu_old_sel[MENU_PLUGINS], game_a);

		SDL_RenderPresent(renderer);

		if (game_a == 0xFF)
			return;
	}
}

static void toggle_selected(void)
{
	const char* path = plugin_at(menu_sel);
	bool enabled;

	if (!path)
		return;

	if (app.read_only)
	{
		show_message("State could not be loaded; saving is disabled.");
		orbisPadUpdate();
		return;
	}

	enabled = pm_model_is_enabled(app.model, app.target_id, path);
	if (pm_model_set_enabled(app.model, app.target_id, path, !enabled) != 0)
	{
		show_message("Out of memory while updating the plugin list");
		orbisPadUpdate();
		return;
	}
	app.dirty = true;
}

/* Persists the toggles. On failure the screen stays open with dirty set so
 * the user can retry; nothing is discarded silently. */
static void save_and_leave(void)
{
	if (app.dirty)
	{
		if (app.read_only || storage_save_state(app.model) != 0)
		{
			/* Yes/no dialog: "No" keeps the screen open so the user can retry,
			 * "Yes" discards the in-memory toggles and reloads from disk. */
			int discard = show_dialog(1,
				"Failed to save. Changes were NOT written to plugins.ini.\n"
				"Discard the changes and go back?");
			orbisPadUpdate();
			if (!discard)
				return;

			pm_model_clear(app.model);
			if (storage_load_state(app.model) != 0)
				app.read_only = true;
			app.dirty = false;
			SetMenu(MENU_TARGETS);
			return;
		}

		show_message("Saved. Changes apply the next time the game is launched.");
		orbisPadUpdate();
		app.dirty = false;
	}

	SetMenu(MENU_TARGETS);
}

void doPluginsMenu(void)
{
	int count = (int)list_count(app.plugins);

	if (count > 0 && orbisPadGetButtonHold(ORBIS_PAD_BUTTON_UP))
		move_selection_back(count, 1);

	else if (count > 0 && orbisPadGetButtonHold(ORBIS_PAD_BUTTON_DOWN))
		move_selection_fwd(count, 1);

	else if (count > 0 && orbisPadGetButtonHold(ORBIS_PAD_BUTTON_LEFT))
		move_selection_back(count, 5);

	else if (count > 0 && orbisPadGetButtonHold(ORBIS_PAD_BUTTON_RIGHT))
		move_selection_fwd(count, 5);

	else if (count > 0 && orbisPadGetButtonHold(ORBIS_PAD_BUTTON_L1))
		move_selection_back(count, 25);

	else if (count > 0 && orbisPadGetButtonHold(ORBIS_PAD_BUTTON_R1))
		move_selection_fwd(count, 25);

	else if (orbisPadGetButtonPressed(ORBIS_PAD_BUTTON_CROSS))
		toggle_selected();

	else if (orbisPadGetButtonPressed(ORBIS_PAD_BUTTON_SQUARE))
	{
		int config_menu = plugins_cursor_config_menu();

		if (config_menu >= 0)
		{
			SetMenu(config_menu);
			return;
		}
	}

	else if (orbisPadGetButtonPressed(ORBIS_PAD_BUTTON_CIRCLE))
	{
		save_and_leave();
		return;
	}

	Draw_PluginsMenu(0xFF);
}
