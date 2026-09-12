/*
 * Lazy, bounded LRU cache of application icons.
 */
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "app.h"
#include "menu.h"
#include "icon_cache.h"

typedef struct
{
	char title_id[16];
	SDL_Texture* texture;   /* NULL = load failed, placeholder is used */
	uint32_t last_used;
	int valid;
} icon_slot_t;

static icon_slot_t slots[ICON_CACHE_CAPACITY];
static SDL_Texture* placeholder_texture = NULL;
static uint32_t tick = 0;

void icon_cache_init(SDL_Texture* placeholder)
{
	memset(slots, 0, sizeof(slots));
	placeholder_texture = placeholder;
	tick = 0;
}

static icon_slot_t* find_slot(const char* title_id)
{
	for (int i = 0; i < ICON_CACHE_CAPACITY; i++)
		if (slots[i].valid && strcmp(slots[i].title_id, title_id) == 0)
			return &slots[i];

	return NULL;
}

static icon_slot_t* evict_slot(void)
{
	icon_slot_t* victim = &slots[0];

	for (int i = 0; i < ICON_CACHE_CAPACITY; i++)
	{
		if (!slots[i].valid)
			return &slots[i];

		if (slots[i].last_used < victim->last_used)
			victim = &slots[i];
	}

	if (victim->texture)
		SDL_DestroyTexture(victim->texture);

	memset(victim, 0, sizeof(*victim));
	return victim;
}

SDL_Texture* icon_cache_get(const char* title_id)
{
	icon_slot_t* slot;
	char path[64];

	if (!title_id || !title_id[0])
		return placeholder_texture;

	slot = find_slot(title_id);
	if (!slot)
	{
		slot = evict_slot();
		strncpy(slot->title_id, title_id, sizeof(slot->title_id) - 1);
		snprintf(path, sizeof(path), APPMETA_ICON_FMT, title_id);
		LOG("icon load '%s'", path);
		slot->texture = LoadTextureFromFile(path);
		LOG("icon load '%s' -> %s", path, slot->texture ? "ok" : "placeholder");
		slot->valid = 1;
	}

	slot->last_used = ++tick;
	return slot->texture ? slot->texture : placeholder_texture;
}

void icon_cache_free(void)
{
	for (int i = 0; i < ICON_CACHE_CAPACITY; i++)
	{
		if (slots[i].texture)
			SDL_DestroyTexture(slots[i].texture);
		memset(&slots[i], 0, sizeof(slots[i]));
	}
}
