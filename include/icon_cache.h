/*
 * Lazy, bounded cache of /user/appmeta/<TITLE_ID>/icon0.png textures.
 */
#ifndef ICON_CACHE_H
#define ICON_CACHE_H

#include <SDL2/SDL.h>

#define ICON_CACHE_CAPACITY 32

/* placeholder is returned for titles whose icon cannot be loaded; not owned. */
void icon_cache_init(SDL_Texture* placeholder);

/* Loads the icon on first request and keeps it while it stays among the
 * ICON_CACHE_CAPACITY most recently used. Never returns NULL. */
SDL_Texture* icon_cache_get(const char* title_id);

void icon_cache_free(void);

#endif /* ICON_CACHE_H */
