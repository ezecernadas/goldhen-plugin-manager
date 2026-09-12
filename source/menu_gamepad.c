/*
 * Gamepad Helper screen: edits the gamepad.ini section of the selected target.
 *
 * Every row maps to one key of the section. The "default" value means the key
 * is absent from the file, so the plugin applies its own default; any other
 * value is written verbatim.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "orbisPad.h"
#include "menu.h"
#include "menu_gui.h"
#include "libfont.h"
#include "storage.h"

#define GP_ROW_HEIGHT       56
#define GP_LIST_Y           (help_png_y + 20)
#define GP_LIST_H           (help_png_h - 40 - 60)
#define GP_MAX_PER_PAGE     (GP_LIST_H / GP_ROW_HEIGHT)
#define GP_LABEL_X          (MENU_ICON_OFF + MENU_TITLE_OFF + 50)
#define GP_VALUE_X          (SCREEN_WIDTH - 180)
#define GP_HINT_Y           (help_png_y + help_png_h - 60)

#define GP_RANGE_MAX        128
#define GP_RANGE_BIG_STEP   10

typedef enum
{
	ROW_ONOFF,      /* "1" / "0" */
	ROW_RANGE,      /* 0 .. GP_RANGE_MAX */
	ROW_BUTTON,     /* one of BUTTON_TOKENS */
	ROW_VIBRATION,  /* one of VIBRATION_TOKENS */
} row_kind_t;

typedef struct
{
	const char* label;
	const char* key;
	row_kind_t kind;
} gp_row_t;

static const char* ONOFF_TOKENS[] = { "1", "0" };

static const char* BUTTON_TOKENS[] = {
	"BUTTON_OPTIONS", "BUTTON_UP", "BUTTON_RIGHT", "BUTTON_DOWN", "BUTTON_LEFT",
	"BUTTON_L3", "BUTTON_R3", "BUTTON_L2", "BUTTON_R2", "BUTTON_L1", "BUTTON_R1",
	"BUTTON_TRIANGLE", "BUTTON_CIRCLE", "BUTTON_CROSS", "BUTTON_SQUARE", "BUTTON_TOUCH_PAD",
};

static const char* VIBRATION_TOKENS[] = { "off", "weak", "medium", "strong" };

static const gp_row_t ROWS[] = {
	{ "Dead zone",        "enableDeadZone",       ROW_ONOFF },
	{ "Dead zone left",   "DeadZoneLeft",         ROW_RANGE },
	{ "Dead zone right",  "DeadZoneRight",        ROW_RANGE },
	{ "Custom touchpad",  "enableCustomTouchPad", ROW_ONOFF },
	{ "Touch L1",         "TOUCH_L1",             ROW_BUTTON },
	{ "Touch R1",         "TOUCH_R1",             ROW_BUTTON },
	{ "Touch L2",         "TOUCH_L2",             ROW_BUTTON },
	{ "Touch R2",         "TOUCH_R2",             ROW_BUTTON },
	{ "Custom buttons",   "enableCustomButton",   ROW_ONOFF },
	{ "Map Options",      "BUTTON_OPTIONS",       ROW_BUTTON },
	{ "Map Up",           "BUTTON_UP",            ROW_BUTTON },
	{ "Map Right",        "BUTTON_RIGHT",         ROW_BUTTON },
	{ "Map Down",         "BUTTON_DOWN",          ROW_BUTTON },
	{ "Map Left",         "BUTTON_LEFT",          ROW_BUTTON },
	{ "Map L3",           "BUTTON_L3",            ROW_BUTTON },
	{ "Map R3",           "BUTTON_R3",            ROW_BUTTON },
	{ "Map L2",           "BUTTON_L2",            ROW_BUTTON },
	{ "Map R2",           "BUTTON_R2",            ROW_BUTTON },
	{ "Map L1",           "BUTTON_L1",            ROW_BUTTON },
	{ "Map R1",           "BUTTON_R1",            ROW_BUTTON },
	{ "Map Triangle",     "BUTTON_TRIANGLE",      ROW_BUTTON },
	{ "Map Circle",       "BUTTON_CIRCLE",        ROW_BUTTON },
	{ "Map Cross",        "BUTTON_CROSS",         ROW_BUTTON },
	{ "Map Square",       "BUTTON_SQUARE",        ROW_BUTTON },
	{ "Map Touchpad",     "BUTTON_TOUCH_PAD",     ROW_BUTTON },
	/* The plugin's own (misspelled) key name; kept verbatim on purpose. */
	{ "Vibration",        "VirationIntensity",    ROW_VIBRATION },
};

#define GP_ROW_COUNT ((int)(sizeof(ROWS) / sizeof(ROWS[0])))

/* Token list of an enumerated row */
static const char* const* row_tokens(const gp_row_t* row, int* count)
{
	switch (row->kind)
	{
		case ROW_ONOFF:
			*count = (int)(sizeof(ONOFF_TOKENS) / sizeof(ONOFF_TOKENS[0]));
			return ONOFF_TOKENS;
		case ROW_BUTTON:
			*count = (int)(sizeof(BUTTON_TOKENS) / sizeof(BUTTON_TOKENS[0]));
			return BUTTON_TOKENS;
		case ROW_VIBRATION:
			*count = (int)(sizeof(VIBRATION_TOKENS) / sizeof(VIBRATION_TOKENS[0]));
			return VIBRATION_TOKENS;
		case ROW_RANGE:
		default:
			*count = 0;
			return NULL;
	}
}

static const char* row_value(const gp_row_t* row)
{
	return gp_get(app.gamepad, app.target_id, row->key);
}

/* Index in the value sequence: 0 = default (absent), 1..n = known tokens,
 * -1 = a value outside the known list. */
static int token_index(const gp_row_t* row, const char* value)
{
	int count;
	const char* const* tokens = row_tokens(row, &count);

	if (!value)
		return 0;

	for (int i = 0; i < count; i++)
		if (strcmp(tokens[i], value) == 0)
			return i + 1;

	return -1;
}

static int store(const gp_row_t* row, const char* value)
{
	const char* current = gp_get(app.gamepad, app.target_id, row->key);

	/* No-op edits must not mark the file dirty: a rewrite drops comments */
	if ((!current && !value) || (current && value && strcmp(current, value) == 0))
		return 0;

	if (gp_set(app.gamepad, app.target_id, row->key, value) != 0)
	{
		show_message("Out of memory while updating gamepad.ini");
		orbisPadUpdate();
		return -1;
	}
	app.gamepad_dirty = true;
	return 0;
}

static void store_range(const gp_row_t* row, int number)
{
	char buf[16];

	if (number < 0)
	{
		store(row, NULL);
		return;
	}
	if (number > GP_RANGE_MAX)
		number = GP_RANGE_MAX;
	snprintf(buf, sizeof(buf), "%d", number);
	store(row, buf);
}

/* Steps a value through its sequence. Enumerated rows: default <-> tokens,
 * no wrap; a value outside the known list moves into the sequence. Range
 * rows: default <-> 0..GP_RANGE_MAX by step. */
static void step_value(const gp_row_t* row, int step)
{
	const char* value = row_value(row);

	if (row->kind == ROW_RANGE)
	{
		int number;

		if (!value)
		{
			if (step > 0)
				store_range(row, 0);
			return;
		}

		number = atoi(value);
		if (number < 0)
			number = 0;
		if (step < 0 && number == 0)
		{
			store(row, NULL);
			return;
		}
		number += step;
		if (number < 0)
			number = 0;
		store_range(row, number);
		return;
	}
	else
	{
		int count;
		const char* const* tokens = row_tokens(row, &count);
		int idx = token_index(row, value);

		if (idx < 0)
			idx = (step > 0) ? 1 : 0;
		else
		{
			idx += (step > 0) ? 1 : -1;
			if (idx < 0)
				idx = 0;
			if (idx > count)
				idx = count;
		}

		store(row, idx == 0 ? NULL : tokens[idx - 1]);
	}
}

/* Cross: on/off rows cycle default -> on -> off -> default; others step forward. */
static void cross_value(const gp_row_t* row)
{
	if (row->kind == ROW_ONOFF)
	{
		const char* value = row_value(row);

		if (!value)
			store(row, "1");
		else if (strcmp(value, "1") == 0)
			store(row, "0");
		else
			store(row, NULL);
		return;
	}

	step_value(row, 1);
}

/* Human-readable form of a value: "default", on/off, "TOUCH PAD", ... */
static void value_display(const gp_row_t* row, const char* value, char* out, size_t out_size)
{
	if (!value)
	{
		snprintf(out, out_size, "default");
		return;
	}

	if (row->kind == ROW_ONOFF && strcmp(value, "1") == 0)
	{
		snprintf(out, out_size, "on");
		return;
	}
	if (row->kind == ROW_ONOFF && strcmp(value, "0") == 0)
	{
		snprintf(out, out_size, "off");
		return;
	}

	if (strncmp(value, "BUTTON_", 7) == 0)
		value += 7;

	snprintf(out, out_size, "%s", value);
	for (char* p = out; *p; p++)
		if (*p == '_')
			*p = ' ';
}

static void draw_row(int index, int y, u8 alpha)
{
	const gp_row_t* row = &ROWS[index];
	const char* value = row_value(row);
	char text[64];

	SetFontColor(APP_FONT_COLOR | alpha, 0);
	DrawString(GP_LABEL_X, y + 4, row->label);

	value_display(row, value, text, sizeof(text));
	SetFontColor((value ? APP_FONT_COLOR : APP_FONT_HINT_COLOR) | alpha, 0);
	SetFontAlign(FONT_ALIGN_RIGHT);
	DrawString(GP_VALUE_X, y + 4, text);
	SetFontAlign(FONT_ALIGN_LEFT);
}

static void draw_list(int selIndex, u8 alpha)
{
	int count = GP_ROW_COUNT;
	int maxPerPage = GP_MAX_PER_PAGE;
	int first = selIndex - (maxPerPage / 2);
	int y = GP_LIST_Y;
	char hint[96];
	int i;

	SetFontSize(APP_FONT_SIZE_SELECTION);

	if (first > count - maxPerPage)
		first = count - maxPerPage;
	if (first < 0)
		first = 0;

	for (i = first; i < first + maxPerPage && i < count; i++)
	{
		if (i == selIndex)
			DrawSelectionMark(y, GP_ROW_HEIGHT, MENU_ICON_OFF + MENU_TITLE_OFF, alpha);

		draw_row(i, y, alpha);

		y += GP_ROW_HEIGHT;
	}

	DrawScrollBar(selIndex, count, maxPerPage, SCREEN_WIDTH - 125, alpha);

	// Persistent hint
	snprintf(hint, sizeof(hint), "Section [%s] of gamepad.ini   Left/Right change, L1/R1 step by 10", app.target_id);
	SetFontSize(APP_FONT_SIZE_SUBTEXT);
	SetFontColor(APP_FONT_HINT_COLOR | alpha, 0);
	SetFontAlign(FONT_ALIGN_SCREEN_CENTER);
	DrawString(0, GP_HINT_Y, hint);
	SetFontAlign(FONT_ALIGN_LEFT);
}

void Draw_GamepadMenu(u8 alpha)
{
	DrawHeader(header_ico_opt_png_index, 0, "Gamepad Helper", app.target_name, APP_FONT_TITLE_COLOR | alpha, 0xffffffff, 0);
	draw_list(menu_sel, alpha);
}

void Draw_GamepadMenu_Ani(void)
{
	int ani;

	for (ani = 0; ani < MENU_ANI_MAX; ani++)
	{
		SDL_RenderClear(renderer);
		DrawHeader_Ani(header_ico_opt_png_index, "Gamepad Helper", app.target_name, APP_FONT_TITLE_COLOR, 0xffffffff, ani, 12);

		u8 icon_a = (u8)(((ani * 2) > 0xFF) ? 0xFF : (ani * 2));
		int _game_a = (int)(icon_a - (MENU_ANI_MAX / 2)) * 2;
		if (_game_a > 0xFF)
			_game_a = 0xFF;
		u8 game_a = (u8)(_game_a < 0 ? 0 : _game_a);

		if (game_a > 0)
			draw_list(0, game_a);

		SDL_RenderPresent(renderer);

		if (game_a == 0xFF)
			return;
	}
}

/* Persists the edits. On failure the screen stays open with the dirty flag
 * set unless the user chooses to discard and reload from disk. */
static void save_and_leave(void)
{
	if (app.gamepad_dirty)
	{
		if (storage_save_gamepad(app.gamepad) != 0)
		{
			int discard = show_dialog(1,
				"Failed to save. Changes were NOT written to gamepad.ini.\n"
				"Discard the changes and go back?");
			orbisPadUpdate();
			if (!discard)
				return;

			gp_model_clear(app.gamepad);
			if (storage_load_gamepad(app.gamepad) != 0)
			{
				show_message("gamepad.ini could not be reloaded; the screen starts empty.");
				orbisPadUpdate();
			}
			app.gamepad_dirty = false;
			SetMenu(MENU_PLUGINS);
			return;
		}

		show_message("Saved. Changes apply the next time the game is launched.");
		orbisPadUpdate();
		app.gamepad_dirty = false;
	}

	SetMenu(MENU_PLUGINS);
}

void doGamepadMenu(void)
{
	const gp_row_t* row = &ROWS[menu_sel];

	if (orbisPadGetButtonHold(ORBIS_PAD_BUTTON_UP))
		move_selection_back(GP_ROW_COUNT, 1);

	else if (orbisPadGetButtonHold(ORBIS_PAD_BUTTON_DOWN))
		move_selection_fwd(GP_ROW_COUNT, 1);

	else if (orbisPadGetButtonHold(ORBIS_PAD_BUTTON_LEFT))
		step_value(row, -1);

	else if (orbisPadGetButtonHold(ORBIS_PAD_BUTTON_RIGHT))
		step_value(row, 1);

	else if (orbisPadGetButtonHold(ORBIS_PAD_BUTTON_L1))
		step_value(row, (row->kind == ROW_RANGE) ? -GP_RANGE_BIG_STEP : -1);

	else if (orbisPadGetButtonHold(ORBIS_PAD_BUTTON_R1))
		step_value(row, (row->kind == ROW_RANGE) ? GP_RANGE_BIG_STEP : 1);

	else if (orbisPadGetButtonPressed(ORBIS_PAD_BUTTON_CROSS))
		cross_value(row);

	else if (orbisPadGetButtonPressed(ORBIS_PAD_BUTTON_CIRCLE))
	{
		save_and_leave();
		return;
	}

	Draw_GamepadMenu(0xFF);
}
