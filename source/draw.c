#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define STBI_ASSERT(x)
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "types.h"
#include "common.h"
#include "libfont.h"
#include "menu.h"

#include <dbglogger.h>
#ifndef LOG
#define LOG dbglogger_log
#endif



int LoadMenuTexture(const char* path, int idx)
{
	int d;

	LOG("Loading '%s'", path);
	if (menu_textures[idx].texture)
		SDL_DestroyTexture(menu_textures[idx].texture);

	menu_textures[idx].size = 0;
	menu_textures[idx].texture = NULL;
	menu_textures[idx].buffer = (uint32_t*) stbi_load(path, &menu_textures[idx].width, &menu_textures[idx].height, &d, STBI_rgb_alpha);

	if (!menu_textures[idx].buffer)
	{
		LOG("Error Loading texture (%s)!", path);
		return 0;
	}

	// GLES2 supports RGBA8888 only
	for (d = 0; d < menu_textures[idx].width * menu_textures[idx].height; d++)
		menu_textures[idx].buffer[d] = ES32(menu_textures[idx].buffer[d]);

	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(menu_textures[idx].buffer, menu_textures[idx].width, menu_textures[idx].height, 32, 4 * menu_textures[idx].width,
												0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);

	menu_textures[idx].texture = SDL_CreateTextureFromSurface(renderer, surface);

	SDL_FreeSurface(surface);
	stbi_image_free(menu_textures[idx].buffer);

	menu_textures[idx].size = menu_textures[idx].width * menu_textures[idx].height * 4;
	menu_textures[idx].buffer = NULL;
	return 1;
}

// draw one background color in virtual 2D coordinates
void DrawBackground2D(u32 rgba)
{
	SDL_SetRenderDrawColor(renderer, RGBA_R(rgba), RGBA_G(rgba), RGBA_B(rgba), RGBA_A(rgba));

	SDL_RenderClear(renderer);
}

static void _drawListBackground(int off, int icon)
{
	(void)off;
	if (icon)
		DrawTexture(&menu_textures[help_png_index], help_png_x, help_png_y, 0, help_png_w, help_png_h, 0xFFFFFF00 | 0xFF);
}

/* Right edge of the header text area. */
#define HEADER_RIGHT_X          (SCREEN_WIDTH - 75)
#define HEADER_TITLE_X          (MENU_ICON_OFF + MENU_TITLE_OFF)
#define HEADER_GAP              30

/* Copies title into out, trimming it (UTF-8 safe, "..." appended) until it
 * fits max_width pixels at the current font size. */
static void fitHeaderTitle(const char* title, char* out, size_t out_size, int max_width)
{
	size_t len = strlen(title);
	int trimmed = 0;

	if (max_width < 0)
		max_width = 0;

	if (len > out_size - 4)
	{
		len = out_size - 4;
		trimmed = 1;
	}
	memcpy(out, title, len);
	out[len] = '\0';
	utf8_truncate(out, len);
	len = strlen(out);

	for (;;)
	{
		if (trimmed)
			memcpy(out + len, "...", 4);

		if (len == 0 || WidthFromStr(out) <= max_width)
			break;

		trimmed = 1;
		utf8_truncate(out, len - 1);
		len = strlen(out);
	}
}

/* Width of the sub title at the sub title font size (0 when absent). */
static int subTitleWidth(const char* headerSubTitle)
{
	int w;

	if (!headerSubTitle)
		return 0;

	SetFontSize(APP_FONT_SIZE_SUBTITLE);
	w = WidthFromStr(headerSubTitle);
	return w;
}

/* Draws the sub title right-aligned, trimmed to the space left by the title. */
static void drawHeaderSubTitle(const char* headerSubTitle, int title_width, u32 rgba)
{
	int width = HEADER_RIGHT_X - (HEADER_TITLE_X + title_width) - HEADER_GAP;
	char* tName;

	if (width < 0)
		width = 0;

	SetFontColor(rgba, 0);
	SetFontSize(APP_FONT_SIZE_SUBTITLE);
	tName = strdup(headerSubTitle);
	if (!tName)
		return;

	while (tName[0] && WidthFromStr(tName) > width)
		utf8_truncate(tName, strlen(tName) - 1);

	SetFontAlign(FONT_ALIGN_RIGHT);
	DrawString(HEADER_RIGHT_X, 35, tName);
	free(tName);
	SetFontAlign(FONT_ALIGN_LEFT);
}

void DrawHeader_Ani(int icon, const char * hdrTitle, const char * headerSubTitle, u32 rgba, u32 bgrgba, int ani, int div)
{
	u8 icon_a = (u8)(((ani * 2) > 0xFF) ? 0xFF : (ani * 2));
	char headerTitle[256];
	int sub_w = subTitleWidth(headerSubTitle);
	int title_w;

	//------------ Backgrounds
	
	//Background
	DrawBackgroundTexture(0, icon, (u8)bgrgba);

	_drawListBackground(0, icon);
	//------------- Menu Bar
	SetFontColor(0x000000 | icon_a, 0);
	SetFontSize(APP_FONT_SIZE_SUBTITLE);
	DrawStringMono(MENU_ICON_OFF + 10, 80, "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4");

	//header mini icon
	DrawTextureCenteredX(&menu_textures[icon], MENU_ICON_OFF - 20, 32, 0, 96, 96, 0xffffff00 | icon_a);

	//header title string
	SetFontColor(rgba | icon_a, 0);
	SetFontSize(APP_FONT_SIZE_TITLE);
	fitHeaderTitle(hdrTitle, headerTitle, sizeof(headerTitle),
		HEADER_RIGHT_X - HEADER_TITLE_X - (sub_w ? sub_w + HEADER_GAP : 0));
	title_w = WidthFromStr(headerTitle);
	DrawString(MENU_ICON_OFF + 40, 31, headerTitle);

	//header sub title string
	if (headerSubTitle)
		drawHeaderSubTitle(headerSubTitle, title_w, rgba | icon_a);
}

void DrawHeader(int icon, int xOff, const char * hdrTitle, const char * headerSubTitle, u32 rgba, u32 bgrgba, int mode)
{
	char headerTitle[256];
	int sub_w = subTitleWidth(headerSubTitle);
	int title_max = HEADER_RIGHT_X - HEADER_TITLE_X - (sub_w ? sub_w + HEADER_GAP : 0);
	int title_w;

	//Background
	DrawBackgroundTexture(xOff, icon, (u8)bgrgba);

	_drawListBackground(xOff, icon);
	//------------ Menu Bar
	SetFontColor(0x000000FF, 0);
	SetFontSize(APP_FONT_SIZE_SUBTITLE);
	DrawStringMono(xOff + MENU_ICON_OFF + 10, 80, "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4");

	//header mini icon
	//header title string
	SetFontColor(rgba, 0);
	if (mode)
	{
		DrawTextureCenteredX(&menu_textures[icon], xOff + MENU_ICON_OFF - 12, 40, 0, 64, 64, 0xffffffff);
		SetFontSize(APP_FONT_SIZE_SUBTITLE);
		fitHeaderTitle(hdrTitle, headerTitle, sizeof(headerTitle), title_max);
		title_w = WidthFromStr(headerTitle);
		DrawString(xOff + MENU_ICON_OFF + 40, 35, headerTitle);
	}
	else
	{
		DrawTextureCenteredX(&menu_textures[icon], xOff + MENU_ICON_OFF - 20, 32, 0, 96, 96, 0xffffffff);
		SetFontSize(APP_FONT_SIZE_TITLE);
		fitHeaderTitle(hdrTitle, headerTitle, sizeof(headerTitle), title_max);
		title_w = WidthFromStr(headerTitle);
		DrawString(xOff + MENU_ICON_OFF + 40, 31, headerTitle);
	}

	//header sub title string
	if (headerSubTitle)
		drawHeaderSubTitle(headerSubTitle, title_w, rgba);
}

void DrawBackgroundTexture(int x, int id, u8 alpha)
{
	(void)id;
	DrawTexture(&menu_textures[bglist_png_index], x, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0xFFFFFF00 | alpha);
}

void DrawTexture(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba)
{
	SDL_Rect dest = {
		.x = x,
		.y = y,
		.w = w,
		.h = h,
	};

	SDL_SetTextureAlphaMod(tex->texture, RGBA_A(rgba));
	SDL_RenderCopy(renderer, tex->texture, NULL, &dest);
}

void DrawTextureCentered(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba)
{
	x -= w / 2;
	y -= h / 2;

	DrawTexture(tex, x, y, z, w, h, rgba);
}

void DrawTextureCenteredX(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba)
{
	x -= w / 2;

	DrawTexture(tex, x, y, z, w, h, rgba);
}

void DrawTextureCenteredY(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba)
{
	y -= h / 2;

	DrawTexture(tex, x, y, z, w, h, rgba);
}

SDL_Texture* LoadTextureFromFile(const char* path)
{
	int w, h, d;
	uint32_t* buffer;
	SDL_Surface* surface;
	SDL_Texture* texture;

	buffer = (uint32_t*) stbi_load(path, &w, &h, &d, STBI_rgb_alpha);
	if (!buffer)
	{
		LOG("Error loading texture (%s)", path);
		return NULL;
	}

	// GLES2 supports RGBA8888 only
	for (d = 0; d < w * h; d++)
		buffer[d] = ES32(buffer[d]);

	surface = SDL_CreateRGBSurfaceFrom(buffer, w, h, 32, 4 * w, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
	texture = surface ? SDL_CreateTextureFromSurface(renderer, surface) : NULL;

	if (surface)
		SDL_FreeSurface(surface);
	stbi_image_free(buffer);

	return texture;
}

void DrawRawTexture(SDL_Texture* tex, int x, int y, int w, int h, u8 alpha)
{
	SDL_Rect dest = {
		.x = x,
		.y = y,
		.w = w,
		.h = h,
	};

	if (!tex)
		return;

	SDL_SetTextureAlphaMod(tex, alpha);
	SDL_RenderCopy(renderer, tex, NULL, &dest);
}

void drawSplashLogo(int mode)
{
	int ani, max;

	if (mode > 0)
	{
		ani = 0;
		max = MENU_ANI_MAX;
	}
	else
	{
		ani = MENU_ANI_MAX;
		max = 0;
	}

	for (; ani != max; ani += mode)
	{
		// clear the current display buffer
		SDL_RenderClear(renderer);
		DrawBackground2D(0x000000FF);

		int logo_a_t = ((ani < 0x20) ? 0 : ((ani - 0x20)*3));
		if (logo_a_t > 0xFF)
			logo_a_t = 0xFF;
		u8 logo_a = (u8)logo_a_t;

		DrawTextureCentered(&menu_textures[pslogo_png_index], SCREEN_WIDTH/2, SCREEN_HEIGHT /2, 0, menu_textures[pslogo_png_index].width, menu_textures[pslogo_png_index].height, 0xFFFFFF00 | logo_a);

		//flush and flip
		SDL_RenderPresent(renderer);
	}
}

void drawEndLogo(void)
{
	SDL_Rect rect = {
		.x = 0,
		.w = SCREEN_WIDTH,
	};

	for (rect.h = 0; rect.h <= SCREEN_HEIGHT/2; rect.h += 3)
	{
		// clear the current display buffer
		SDL_RenderClear(renderer);
		DrawBackground2D(0xFFFFFFFF);

		DrawTextureCentered(&menu_textures[titlescr_logo_png_index], SCREEN_WIDTH/2, SCREEN_HEIGHT /2, 0, menu_textures[titlescr_logo_png_index].width, menu_textures[titlescr_logo_png_index].height, 0xFFFFFF00 | 0xFF);

		rect.y = 0;
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
		SDL_RenderFillRect(renderer, &rect);

		rect.y = SCREEN_HEIGHT - rect.h;
		SDL_RenderFillRect(renderer, &rect);

		//flush and flip
		SDL_RenderPresent(renderer);
	}
}
