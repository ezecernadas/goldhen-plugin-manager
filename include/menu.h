#ifndef __PLGM_MENU_H__
#define __PLGM_MENU_H__

#include <SDL2/SDL.h>

#include "types.h"
#include "structs.h"

// SDL window and software renderer
extern SDL_Window* window;
extern SDL_Renderer* renderer;

#define MENU_TITLE_OFF             45          // Offset of menu title text from menu mini icon
#define MENU_ICON_OFF              105         // X Offset to start printing menu mini icon
#define MENU_ANI_MAX               0x80        // Max animation number
#define OPTION_ITEM_OFF            1595        // Offset from left of settings item/value

enum menu_screen_ids
{
	MENU_TARGETS,		/* 0 - Global + installed games */
	MENU_PLUGINS,		/* 1 - Plugin checkboxes for the selected target */
	MENU_GAMEPAD,		/* 2 - Gamepad Helper settings for the selected target */
	TOTAL_MENU_IDS
};

//Textures
enum texture_index
{
	bglist_png_index,
	pslogo_png_index,
	footer_ico_circle_png_index,
	footer_ico_cross_png_index,
	footer_ico_square_png_index,
	header_ico_opt_png_index,
	header_ico_xmb_png_index,
	help_png_index,
	mark_arrow_png_index,
	mark_line_png_index,
	opt_off_png_index,
	opt_on_png_index,
	scroll_bg_png_index,
	scroll_lock_png_index,
	titlescr_ico_opt_png_index,
	titlescr_ico_cht_png_index,
	titlescr_logo_png_index,

	TOTAL_MENU_TEXTURES
};

#define RGBA_R(c)		(uint8_t)((c & 0xFF000000) >> 24)
#define RGBA_G(c)		(uint8_t)((c & 0x00FF0000) >> 16)
#define RGBA_B(c)		(uint8_t)((c & 0x0000FF00) >> 8)
#define RGBA_A(c)		(uint8_t) (c & 0x000000FF)

//Fonts
#define  font_console_regular				0

#define APP_FONT_COLOR						0xFFFFFF00
#define APP_FONT_MENU_COLOR					0x00000000
#define APP_FONT_TITLE_COLOR				0x00000000
#define APP_FONT_HINT_COLOR					0xC0C0C000
#define APP_FONT_SIZE_TITLE					84, 72
#define APP_FONT_SIZE_SUBTITLE				68, 60
#define APP_FONT_SIZE_SUBTEXT				36, 36
#define APP_FONT_SIZE_SELECTION				56, 48
#define APP_FONT_SIZE_DESCRIPTION			54, 48
#define APP_LINE_OFFSET						40

#define SCREEN_WIDTH						1920
#define SCREEN_HEIGHT						1080

//Asset sizes
#define help_png_x							80
#define help_png_y							150
#define help_png_w							1730
#define help_png_h							800

typedef struct t_png_texture
{
	uint32_t *buffer;
	int width;
	int height;
	u32 size;
	SDL_Texture *texture;
} png_texture;

extern u32 * texture_mem;      // Pointers to texture memory
extern u32 * free_mem;         // Pointer after last texture

extern png_texture * menu_textures;				// png_texture array for main menu, initialized in LoadTexture

extern int close_app;
extern int menu_id;
extern int menu_sel;
extern int menu_old_sel[];

// draw.c
void DrawBackground2D(u32 rgba);
void DrawTexture(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba);
void DrawTextureCentered(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba);
void DrawTextureCenteredX(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba);
void DrawTextureCenteredY(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba);
void DrawRawTexture(SDL_Texture* tex, int x, int y, int w, int h, u8 alpha);
void DrawHeader(int icon, int xOff, const char * headerTitle, const char * headerSubTitle, u32 rgba, u32 bgrgba, int mode);
void DrawHeader_Ani(int icon, const char * headerTitle, const char * headerSubTitle, u32 rgba, u32 bgrgba, int ani, int div);
void DrawBackgroundTexture(int x, int id, u8 alpha);
int LoadMenuTexture(const char* path, int idx);
SDL_Texture* LoadTextureFromFile(const char* path);
void drawSplashLogo(int m);
void drawEndLogo(void);

// menu.c
void SetMenu(int id);
void drawScene(void);
void move_selection_back(int count, int steps);
void move_selection_fwd(int count, int steps);
void DrawScrollBar(int selIndex, int max, int maxPerPage, int xOff, u8 alpha);
void DrawSelectionMark(int y, int y_inc, int arrow_x, u8 alpha);

// orbis_jbc.c
int initialize_jbc(void);
void terminate_jbc(void);

#endif
