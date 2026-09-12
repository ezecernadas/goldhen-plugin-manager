/*
	GoldHEN Plugin Manager main.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <orbis/Sysmodule.h>
#include <orbis/CommonDialog.h>
#include <orbis/SystemService.h>

#include "app.h"
#include "util.h"
#include "common.h"
#include "orbisPad.h"
#include "app_db.h"
#include "icon_cache.h"
#include "storage.h"

//Menus
#include "menu.h"
#include "menu_gui.h"

//Font
#include "libfont.h"
#include "ttf_render.h"
#include "font-16x32.h"


#define load_menu_texture(name, type) \
			if (!LoadMenuTexture(APP_ASSETS_PATH "images/" #name "." #type , name##_##type##_index)) return 0;

pm_app_t app = {
	.model = NULL,
	.gamepad = NULL,
	.gamepad_dirty = false,
	.installed = NULL,
	.plugins = NULL,
	.target_id = PM_TARGET_DEFAULT,
	.target_name = "Global",
	.dirty = false,
	.read_only = false,
};

int close_app = 0;

png_texture * menu_textures;                // png_texture array for main menu, initialized in LoadTexture
SDL_Window* window;                         // SDL window
SDL_Renderer* renderer;                     // SDL software renderer
uint32_t* texture_mem;                      // Pointers to texture memory
uint32_t* free_mem;                         // Pointer after last texture

static const char* get_button_prompts(int id)
{
	switch (id)
	{
		case MENU_PLUGINS:
			return (plugins_cursor_config_menu() >= 0)
				? "\x10 Toggle    \x11 Configure    \x13 Save & Back"
				: "\x10 Toggle    \x13 Save & Back";

		case MENU_GAMEPAD:
			return "\x10 Change    \x13 Save & Back";

		case MENU_LANGUAGE:
			return "\x10 Select    \x13 Back";

		case MENU_TARGETS:
		default:
			return "\x10 Select    \x13 Exit";
	}
}

static int initPad(void)
{
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_PAD) < 0)
		return 0;

	// Initialize the Pad library
	if (orbisPadInit() < 0)
	{
		LOG("[ERROR] Failed to initialize pad library!");
		return 0;
	}

	return 1;
}

// Used only in initialization. Allocates memory for textures and loads the fonts
static int LoadTextures_Menu(void)
{
	texture_mem = malloc(256 * 32 * 32 * 4);
	menu_textures = (png_texture *)calloc(TOTAL_MENU_TEXTURES, sizeof(png_texture));

	if(!texture_mem || !menu_textures)
		return 0; // fail!

	ResetFont();
	free_mem = (u32 *) AddFontFromBitmapArray((u8 *) console_font_16x32, (u8 *) texture_mem, 0, 0xFF, 16, 32, 1, BIT7_FIRST_PIXEL);

	if (TTFLoadFont(0, "/preinst/common/font/DFHEI5-SONY.ttf", NULL, 0) != SUCCESS ||
		TTFLoadFont(1, "/system_ex/app/NPXS20113/bdjstack/lib/fonts/SCE-PS3-RD-R-LATIN.TTF", NULL, 0) != SUCCESS)
		return 0;

	free_mem = (u32*) init_ttf_table((u8*) free_mem);
	set_ttf_window(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, WIN_SKIP_LF);

	//Init Menu textures
	load_menu_texture(bglist, png);
	load_menu_texture(pslogo, png);
	load_menu_texture(footer_ico_circle, png);
	load_menu_texture(footer_ico_cross, png);
	load_menu_texture(footer_ico_square, png);
	load_menu_texture(header_ico_opt, png);
	load_menu_texture(header_ico_xmb, png);
	load_menu_texture(help, png);
	load_menu_texture(mark_arrow, png);
	load_menu_texture(mark_line, png);
	load_menu_texture(opt_off, png);
	load_menu_texture(opt_on, png);
	load_menu_texture(scroll_bg, png);
	load_menu_texture(scroll_lock, png);
	load_menu_texture(titlescr_ico_opt, png);
	load_menu_texture(titlescr_ico_cht, png);
	load_menu_texture(titlescr_logo, png);

	u32 tBytes = free_mem - texture_mem;
	LOG("LoadTextures_Menu() :: Allocated %db (%.02fkb, %.02fmb) for textures", tBytes, tBytes / (float)1024, tBytes / (float)(1024 * 1024));
	return 1;
}

static void registerSpecialChars(void)
{
	// Register button icons
	RegisterSpecialCharacter(orbisPadGetConf()->crossButtonOK ? CHAR_BTN_X : CHAR_BTN_O, 0, 1.0, &menu_textures[footer_ico_cross_png_index]);
	RegisterSpecialCharacter(orbisPadGetConf()->crossButtonOK ? CHAR_BTN_O : CHAR_BTN_X, 0, 1.0, &menu_textures[footer_ico_circle_png_index]);
	RegisterSpecialCharacter(CHAR_BTN_S, 0, 1.0, &menu_textures[footer_ico_square_png_index]);
}

static void helpFooter(int id)
{
	u8 alpha = 0xFF;

	if (orbisPadGetConf()->idle > 0x100)
	{
		int dec = (orbisPadGetConf()->idle - 0x100) * 2;
		alpha = (dec > alpha) ? 0 : (alpha - dec);
	}

	SetFontSize(APP_FONT_SIZE_DESCRIPTION);
	SetFontAlign(FONT_ALIGN_SCREEN_CENTER);
	SetFontColor(APP_FONT_MENU_COLOR | alpha, 0);
	DrawString(0, SCREEN_HEIGHT - 94, get_button_prompts(id));
	SetFontAlign(FONT_ALIGN_LEFT);
}

static void terminate(void)
{
	LOG("Exiting...");

	terminate_jbc();
	LOG("Shutdown: LoadExec(exit)");
	sceSystemServiceLoadExec("exit", NULL);
}

static int initInternal(void)
{
	// load common modules
	int ret = sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE);
	if (ret != SUCCESS) {
		LOG("load module failed: SYSTEM_SERVICE (0x%08x)\n", ret);
		return 0;
	}

	ret = sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_USER_SERVICE);
	if (ret != SUCCESS) {
		LOG("load module failed: USER_SERVICE (0x%08x)\n", ret);
		return 0;
	}

	return 1;
}

// Loads plugin state, installed apps and the plugin directory listing
static int pm_app_init(void)
{
	app.model = pm_model_new();
	if (!app.model)
		return 0;

	if (storage_load_state(app.model) != 0)
	{
		app.read_only = true;
		show_message("State could not be loaded; saving is disabled.");
	}

	app.gamepad = gp_model_new();
	if (!app.gamepad)
		return 0;
	if (storage_load_gamepad(app.gamepad) != 0)
		show_message("gamepad.ini could not be read; the Gamepad Helper screen starts empty.");

	app.installed = appdb_load_installed();
	if (!app.installed)
		return 0;
	LOG("app.db: %d installed apps", (int)list_count(app.installed));

	app.plugins = list_alloc();
	if (!app.plugins)
		return 0;
	storage_list_prx(app.plugins);
	LOG("plugins dir: %d .prx files", (int)list_count(app.plugins));

	if (list_count(app.installed) == 0)
		show_message("No installed games were found in app.db.\nOnly the Global section can be edited.");

	icon_cache_init(menu_textures[titlescr_ico_cht_png_index].texture);
	LOG("icon cache ready");

	// Consume the button press that dismissed any dialog shown above
	orbisPadUpdate();
	return 1;
}

static void pm_app_free(void)
{
	icon_cache_free();
	storage_free_prx(app.plugins);
	appdb_free(app.installed);
	pm_model_free(app.model);
	gp_model_free(app.gamepad);
	app.plugins = NULL;
	app.installed = NULL;
	app.model = NULL;
	app.gamepad = NULL;
}


/*
	Program start
*/
s32 main(s32 argc, const char* argv[])
{
	(void)argc;
	(void)argv;
#ifdef DEBUG_ENABLE_LOG
	// Frame tracking info for debugging
	uint32_t lastFrameTicks  = 0;
	uint32_t startFrameTicks = 0;
	uint32_t deltaFrameTicks = 0;

	dbglogger_init_str(DEBUG_LOG_DEST);
	LOG("GoldHEN Plugin Manager " APP_VERSION " starting");
#endif

	// Initialize SDL functions
	LOG("Initializing SDL");

	if (SDL_Init(SDL_INIT_VIDEO) != SUCCESS)
	{
		LOG("Failed to initialize SDL: %s", SDL_GetError());
		return (-1);
	}

	LOG("SDL_Init OK");

	if (!initInternal())
	{
		LOG("Failed to initialize system services!");
		return (-1);
	}
	LOG("System services OK");

	if (!initPad())
	{
		LOG("Failed to initialize the pad!");
		return (-1);
	}
	LOG("Pad OK");

	// Create a window context
	LOG( "Creating a window");
	window = SDL_CreateWindow("main", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
	if (!window) {
		LOG("SDL_CreateWindow: %s", SDL_GetError());
		return (-1);
	}

	// Create a renderer (OpenGL ES2)
	renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!renderer) {
		LOG("SDL_CreateRenderer: %s", SDL_GetError());
		return (-1);
	}

	// Initialize jailbreak
	if (!initialize_jbc())
		terminate();

	// register exit callback right away so every early return drops root creds
	atexit(terminate);

	mkdirs(GOLDHEN_PLUGINS_DIR);

	// Load freetype
	if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL) < 0)
	{
		LOG("Failed to load freetype!");
		return (-1);
	}

	// Load MsgDialog
	if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_MESSAGE_DIALOG) < 0)
	{
		LOG("Failed to load dialog!");
		return (-1);
	}

	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_COMMON_DIALOG) < 0 ||
		sceCommonDialogInitialize() < 0)
	{
		LOG("Failed to init CommonDialog!");
		return (-1);
	}

	// Load texture
	if (!LoadTextures_Menu())
	{
		LOG("Failed to load menu textures!");
		return (-1);
	}

#ifndef DEBUG_ENABLE_LOG
	// Splash screen logo (fade-in)
	drawSplashLogo(1);
#endif

	// Setup font
	SetExtraSpace(-1);
	SetCurrentFont(font_console_regular);
	registerSpecialChars();

	if (!pm_app_init())
	{
		LOG("Failed to initialize application state!");
		return (-1);
	}

#ifndef DEBUG_ENABLE_LOG
	// Splash screen logo (fade-out)
	drawSplashLogo(-1);
#endif
	SDL_DestroyTexture(menu_textures[pslogo_png_index].texture);
	menu_textures[pslogo_png_index].texture = NULL;

	LOG("Entering targets menu");
	SetMenu(MENU_TARGETS);
	LOG("Main loop start");

	while (!close_app)
	{
#ifdef DEBUG_ENABLE_LOG
		static int frame_no = 0;
		if (frame_no < 3)
			LOG("frame %d begin", frame_no);
#endif
#ifdef DEBUG_ENABLE_LOG
		startFrameTicks = SDL_GetTicks();
		deltaFrameTicks = startFrameTicks - lastFrameTicks;
		lastFrameTicks  = startFrameTicks;
#endif
		// Clear the canvas
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
		SDL_RenderClear(renderer);
		orbisPadUpdate();
		drawScene();

		// Draw help
		helpFooter(menu_id);

#ifdef DEBUG_ENABLE_LOG
		// Calculate FPS and ms/frame
		SetFontColor(APP_FONT_COLOR | 0xFF, 0);
		DrawFormatString(50, 960, "FPS: %d", (deltaFrameTicks ? 1000 / deltaFrameTicks : 0));
#endif
		// Propagate the updated window to the screen
		SDL_RenderPresent(renderer);
#ifdef DEBUG_ENABLE_LOG
		if (frame_no < 3)
			LOG("frame %d end", frame_no);
		frame_no++;
#endif
	}

#ifndef DEBUG_ENABLE_LOG
	drawEndLogo();
#endif

	// Cleanup resources
	LOG("Shutdown: freeing app state");
	pm_app_free();
	LOG("Shutdown: pad finish");
	orbisPadFinish();

	/* SDL_DestroyRenderer() of the SDL-PS4 fork crashes (CE-34878-0) on the
	 * console during GLES teardown, even when no texture was destroyed by
	 * hand (verified with on-console logs). The process is replaced by
	 * sceSystemServiceLoadExec("exit") from terminate() right after main()
	 * returns, so the renderer, window and Piglet context are reclaimed by
	 * the system; skip SDL_DestroyRenderer/SDL_DestroyWindow/SDL_Quit. */
	LOG("Shutdown: main returns");
	return 0;
}
