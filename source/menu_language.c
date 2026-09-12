/*
 * System Language screen: picks the language code passed as the argument of
 * system_language_override.prx for the selected target.
 *
 * The plugin reads its own line of plugins.ini ("path ; code"), so choosing a
 * language only edits the plugin arguments in the model; the plugins screen
 * persists them when it saves. The code list mirrors the plugin's README.
 */
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "orbisPad.h"
#include "menu.h"
#include "menu_gui.h"
#include "libfont.h"

#define LANG_ROW_HEIGHT     56
#define LANG_LIST_Y         (help_png_y + 20)
#define LANG_LIST_H         (help_png_h - 40 - 100)   /* two hint lines */
#define LANG_MAX_PER_PAGE   (LANG_LIST_H / LANG_ROW_HEIGHT)
#define LANG_NAME_X         (MENU_ICON_OFF + MENU_TITLE_OFF + 50)
#define LANG_CODE_X         (SCREEN_WIDTH - 180)
#define LANG_HINT1_Y        (help_png_y + help_png_h - 96)
#define LANG_HINT2_Y        (help_png_y + help_png_h - 60)

#define LANGUAGE_PRX_PATH   GOLDHEN_PLUGINS_DIR SYSTEM_LANGUAGE_PRX

/* Path of the plugin entry being edited; captured from the plugin list
 * cursor when the screen is entered, falls back to the canonical name. */
static char plugin_path[256] = LANGUAGE_PRX_PATH;

typedef struct
{
	const char* name;
	const char* code;   /* NULL = no override (argument removed) */
} lang_row_t;

static const lang_row_t ROWS[] = {
	{ "Default (no override)",     NULL    },
	{ "Arabic",                    "ar"    },
	{ "Chinese (Simplified)",      "zh_CN" },
	{ "Chinese (Traditional)",     "zh_TW" },
	{ "Czech",                     "cs"    },
	{ "Danish",                    "da"    },
	{ "Dutch",                     "nl"    },
	{ "English (United Kingdom)",  "en_GB" },
	{ "English (United States)",   "en_US" },
	{ "Finnish",                   "fi"    },
	{ "French (Canada)",           "fr_CA" },
	{ "French (France)",           "fr_FR" },
	{ "German",                    "de"    },
	{ "Greek",                     "el"    },
	{ "Hungarian",                 "hu"    },
	{ "Indonesian",                "in"    },
	{ "Italian",                   "it"    },
	{ "Japanese",                  "ja"    },
	{ "Korean",                    "ko"    },
	{ "Norwegian",                 "no"    },
	{ "Polish",                    "pl"    },
	{ "Portuguese (Brazil)",       "pt_BR" },
	{ "Portuguese (Portugal)",     "pt_PT" },
	{ "Romanian",                  "ro"    },
	{ "Russian",                   "ru"    },
	{ "Spanish (Latin America)",   "es_LA" },
	{ "Spanish (Spain)",           "es_ES" },
	{ "Swedish",                   "sv"    },
	{ "Thai",                      "th"    },
	{ "Turkish",                   "tr"    },
	{ "Ukrainian",                 "uk"    },
	{ "Vietnamese",                "vi"    },
};

#define LANG_ROW_COUNT ((int)(sizeof(ROWS) / sizeof(ROWS[0])))

static const char* current_code(void)
{
	return pm_model_get_args(app.model, app.target_id, plugin_path);
}

/* Row whose code equals the current argument; 0 when there is no argument;
 * -1 when the argument matches no known code. */
static int current_row(void)
{
	const char* code = current_code();
	int i;

	if (!code)
		return 0;

	for (i = 1; i < LANG_ROW_COUNT; i++)
		if (strcmp(ROWS[i].code, code) == 0)
			return i;

	return -1;
}

int language_menu_initial_row(void)
{
	const char* cursor = plugins_cursor_path();
	int row;

	snprintf(plugin_path, sizeof(plugin_path), "%s", cursor ? cursor : LANGUAGE_PRX_PATH);
	row = current_row();

	return row < 0 ? 0 : row;
}

static void draw_row(int index, int y, int marked, u8 alpha)
{
	const lang_row_t* row = &ROWS[index];
	int c = marked ? opt_on_png_index : opt_off_png_index;

	SetFontColor(APP_FONT_COLOR | alpha, 0);
	DrawString(LANG_NAME_X, y + 4, row->name);

	if (row->code)
	{
		SetFontColor(APP_FONT_HINT_COLOR | alpha, 0);
		SetFontAlign(FONT_ALIGN_RIGHT);
		DrawString(LANG_CODE_X, y + 4, row->code);
		SetFontAlign(FONT_ALIGN_LEFT);
	}

	DrawTexture(&menu_textures[c], OPTION_ITEM_OFF - 29, y + 4, 0, menu_textures[c].width, menu_textures[c].height, 0xFFFFFF00 | alpha);
}

static void draw_hints(u8 alpha)
{
	const char* code = current_code();
	char line[160];

	SetFontSize(APP_FONT_SIZE_SUBTEXT);
	SetFontColor(APP_FONT_HINT_COLOR | alpha, 0);
	SetFontAlign(FONT_ALIGN_SCREEN_CENTER);

	if (code && current_row() < 0)
		snprintf(line, sizeof(line), "Current value: \"%.20s\" (unknown code)", code);
	else if (!pm_model_is_enabled(app.model, app.target_id, plugin_path))
		snprintf(line, sizeof(line), "Plugin disabled for this target: enable it in the plugin list");
	else
		snprintf(line, sizeof(line), "Changes are saved when leaving the plugin list");

	DrawString(0, LANG_HINT1_Y, line);
	DrawString(0, LANG_HINT2_Y, "Incompatible with button_swap.prx");
	SetFontAlign(FONT_ALIGN_LEFT);
}

static void draw_list(int selIndex, u8 alpha)
{
	int count = LANG_ROW_COUNT;
	int maxPerPage = LANG_MAX_PER_PAGE;
	int first = selIndex - (maxPerPage / 2);
	int marked = current_row();
	int y = LANG_LIST_Y;
	int i;

	SetFontSize(APP_FONT_SIZE_SELECTION);

	if (first > count - maxPerPage)
		first = count - maxPerPage;
	if (first < 0)
		first = 0;

	for (i = first; i < first + maxPerPage && i < count; i++)
	{
		if (i == selIndex)
			DrawSelectionMark(y, LANG_ROW_HEIGHT, MENU_ICON_OFF + MENU_TITLE_OFF, alpha);

		draw_row(i, y, i == marked, alpha);

		y += LANG_ROW_HEIGHT;
	}

	DrawScrollBar(selIndex, count, maxPerPage, SCREEN_WIDTH - 125, alpha);
	draw_hints(alpha);
}

void Draw_LanguageMenu(u8 alpha)
{
	DrawHeader(header_ico_opt_png_index, 0, "System Language", app.target_name, APP_FONT_TITLE_COLOR | alpha, 0xffffffff, 0);
	draw_list(menu_sel, alpha);
}

void Draw_LanguageMenu_Ani(void)
{
	int ani;

	for (ani = 0; ani < MENU_ANI_MAX; ani++)
	{
		SDL_RenderClear(renderer);
		DrawHeader_Ani(header_ico_opt_png_index, "System Language", app.target_name, APP_FONT_TITLE_COLOR, 0xffffffff, ani, 12);

		u8 icon_a = (u8)(((ani * 2) > 0xFF) ? 0xFF : (ani * 2));
		int _game_a = (int)(icon_a - (MENU_ANI_MAX / 2)) * 2;
		if (_game_a > 0xFF)
			_game_a = 0xFF;
		u8 game_a = (u8)(_game_a < 0 ? 0 : _game_a);

		if (game_a > 0)
			draw_list(menu_old_sel[MENU_LANGUAGE], game_a);

		SDL_RenderPresent(renderer);

		if (game_a == 0xFF)
			return;
	}
}

/* Applies the selected row to the model and returns to the plugin list. The
 * plugins screen persists the change when the user leaves it. */
static void select_row(int index)
{
	const char* code = ROWS[index].code;
	const char* current = current_code();
	int changed = (!code != !current) || (code && current && strcmp(code, current) != 0);

	if (!changed)
	{
		SetMenu(MENU_PLUGINS);
		return;
	}

	if (app.read_only)
	{
		show_message("State could not be loaded; saving is disabled.");
		orbisPadUpdate();
		return;
	}

	{
		if (pm_model_set_args(app.model, app.target_id, plugin_path, code) != 0)
		{
			show_message("Out of memory while updating the plugin arguments");
			orbisPadUpdate();
			return;
		}
		app.dirty = true;
	}

	SetMenu(MENU_PLUGINS);
}

void doLanguageMenu(void)
{
	if (orbisPadGetButtonHold(ORBIS_PAD_BUTTON_UP))
		move_selection_back(LANG_ROW_COUNT, 1);

	else if (orbisPadGetButtonHold(ORBIS_PAD_BUTTON_DOWN))
		move_selection_fwd(LANG_ROW_COUNT, 1);

	else if (orbisPadGetButtonHold(ORBIS_PAD_BUTTON_LEFT))
		move_selection_back(LANG_ROW_COUNT, 5);

	else if (orbisPadGetButtonHold(ORBIS_PAD_BUTTON_RIGHT))
		move_selection_fwd(LANG_ROW_COUNT, 5);

	else if (orbisPadGetButtonPressed(ORBIS_PAD_BUTTON_CROSS))
	{
		select_row(menu_sel);
		return;
	}

	else if (orbisPadGetButtonPressed(ORBIS_PAD_BUTTON_CIRCLE))
	{
		SetMenu(MENU_PLUGINS);
		return;
	}

	Draw_LanguageMenu(0xFF);
}
