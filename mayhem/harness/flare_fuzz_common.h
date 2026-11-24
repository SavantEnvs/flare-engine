// Shared set-up for the flare-engine fuzz harnesses.
//
// flare reads all of its game data through FileParser/GetText, which open files located by
// ModManager::locate() (there is no in-memory API). Each harness therefore:
//   * once, in LLVMFuzzerInitialize: builds a scratch data dir under $TMPDIR with
//       data/mods/default -> the repo's mods/default (the mandatory fallback mod, read-only)
//       data/mods/fuzz/                   (the mod the fuzz input is written into)
//     and creates the engine globals in the same order as src/main.cpp:init(), with SDL's
//     dummy video/audio drivers and the software renderer;
//   * per input: writes the bytes to one file of the "fuzz" mod (always the same name, so the
//     ModManager location cache stays valid) and calls the engine's own loader for that file.
// Everything kept across inputs is either read-only engine configuration from mods/default or is
// reset/re-created per input by the harness (the campaign status map and the item table, below).
#ifndef FLARE_FUZZ_COMMON_H
#define FLARE_FUZZ_COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>

#include <SDL.h>

#include "AnimationManager.h"
#include "CampaignManager.h"
#include "EventManager.h"
#include "ItemManager.h"
#include "LootManager.h"
#include "PowerManager.h"
#include "CombatText.h"
#include "DeviceList.h"
#include "EngineSettings.h"
#include "FontEngine.h"
#include "InputState.h"
#include "MessageEngine.h"
#include "ModManager.h"
#include "RenderDevice.h"
#include "SaveLoad.h"
#include "Settings.h"
#include "SharedGameResources.h"
#include "SharedResources.h"
#include "SoundManager.h"
#include "Stats.h"
#include "TooltipManager.h"
#include "Utils.h"

// main.cpp provides the Platform object by including the platform source; do the same here.
#define PLATFORM_CPP_INCLUDE
#include "PlatformLinux.cpp"

#ifndef FLARE_DEFAULT_MOD_DIR
#error "FLARE_DEFAULT_MOD_DIR must point at the repo's mods/default"

#endif

static std::string g_fuzz_root;   // <tmp>/flare-fuzz-XXXXXX/
static std::string g_fuzz_mod;    // <root>data/mods/fuzz/

static void flare_mkdirs(const std::string& path) {
	for (size_t i = 1; i <= path.size(); ++i) {
		if (i == path.size() || path[i] == '/')
			mkdir(path.substr(0, i).c_str(), 0700);
	}
}

static void flare_write_file(const std::string& path, const uint8_t* data, size_t size) {
	size_t slash = path.rfind('/');
	if (slash != std::string::npos) flare_mkdirs(path.substr(0, slash));
	FILE* f = fopen(path.c_str(), "wb");
	if (!f) { perror(path.c_str()); abort(); }
	if (size && fwrite(data, 1, size, f) != size) { perror(path.c_str()); abort(); }
	fclose(f);
}

// Write a file of the "fuzz" mod, relative to the mod root (e.g. "items/items.txt").
static void flare_write_mod_file(const std::string& rel, const uint8_t* data, size_t size) {
	flare_write_file(g_fuzz_mod + rel, data, size);
}
static void flare_write_mod_text(const std::string& rel, const std::string& text) {
	flare_write_mod_file(rel, reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

// Install a placeholder image into the fuzz mod for an image the engine loads with ERROR_EXIT
// but mods/default does not ship (the game data repo provides it); any default PNG will do.
static void flare_install_placeholder_image(const std::string& rel) {
	FILE* f = fopen(FLARE_DEFAULT_MOD_DIR "/images/menus/tab_active.png", "rb");
	if (!f) { perror("placeholder image"); abort(); }
	std::vector<uint8_t> png;
	uint8_t buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) png.insert(png.end(), buf, buf + n);
	fclose(f);
	flare_write_mod_file(rel, png.empty() ? NULL : &png[0], png.size());
}

// Init level: what the harness needs beyond the core (settings/mods/msg/eset).
enum { FLARE_INIT_CORE = 0, FLARE_INIT_MEDIA = 1 };

// `pre_msg` runs after the mod manager exists but before MessageEngine/EngineSettings read the
// mods, so a harness can install fixture files they depend on.
static void flare_fuzz_init(int level, void (*pre_msg)() = NULL) {
	setenv("SDL_VIDEODRIVER", "dummy", 1);
	setenv("SDL_AUDIODRIVER", "dummy", 1);
	// Silence per-line parse warnings (stderr) and keep Utils from queueing every log line
	// in memory until a log file is created (it never is here).
	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_CRITICAL);
	Utils::LOG_FILE_INIT = true;
	Utils::LOG_FILE_CREATED = false;

	const char* tmp = getenv("TMPDIR");
	std::string tmpl = std::string(tmp && *tmp ? tmp : "/tmp") + "/flare-fuzz-XXXXXX";
	std::vector<char> buf(tmpl.begin(), tmpl.end());
	buf.push_back('\0');
	if (!mkdtemp(&buf[0])) { perror("mkdtemp"); abort(); }
	g_fuzz_root = std::string(&buf[0]) + "/";
	flare_mkdirs(g_fuzz_root + "data/mods");
	if (symlink(FLARE_DEFAULT_MOD_DIR, (g_fuzz_root + "data/mods/default").c_str()) != 0) {
		perror("symlink"); abort();
	}
	g_fuzz_mod = g_fuzz_root + "data/mods/fuzz/";
	flare_mkdirs(g_fuzz_mod);
	flare_mkdirs(g_fuzz_root + "conf");
	flare_mkdirs(g_fuzz_root + "user/mods");
	flare_mkdirs(g_fuzz_root + "user/saves");

	settings = new Settings();
	settings->path_conf = g_fuzz_root + "conf/";
	settings->path_user = g_fuzz_root + "user/";
	settings->path_data = g_fuzz_root + "data/";

	{
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
			fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
			abort();
		}
	}

	static std::vector<std::string> mod_list(1, "fuzz");
	mods = new ModManager(&mod_list);
	if (!mods->haveFallbackMod()) { fprintf(stderr, "no default mod\n"); abort(); }
	if (pre_msg) pre_msg();

	settings->loadSettings();
	settings->audio = false;
	settings->language = "en";

	save_load = new SaveLoad();
	msg = new MessageEngine();
	font = getFontEngine();
	anim = new AnimationManager();
	comb = new CombatText();
	eset = new EngineSettings();
	eset->load();
	Stats::init();

	if (level >= FLARE_INIT_MEDIA) {
		inpt = getInputManager();
		render_device = getRenderDevice("sdl");
		if (render_device->createContext() == -1) {
			fprintf(stderr, "createContext: %s\n", SDL_GetError());
			abort();
		}
		render_device->reloadGraphics();
		snd = getSoundManager();
		tooltipm = new TooltipManager();
	}
}

// The game-level tables the loaders cross-reference (GameStatePlay creates them in this order).
// With no items/powers/loot files in mods/default they are empty tables, so every reference
// from the fuzzed file to an item/power id is an unknown id. The power and loot tables are read
// only during fuzzing. The campaign is not: parsing a status key (requires_status, set_status,
// random_status, loot/enemy/item/power-tree status requirements) goes through
// CampaignManager::parseStatus/registerStatus, which adds the string to camp->status. Every target
// that creates the game tables therefore calls flare_reset_campaign() after each input.
// The item table is not read only either: ItemManager::verifyID(...,
// VERIFY_ALLOCATE) (save files via ItemStorage::setItems, old-syntax reward_item=<id> events via
// EventManager::loadEventComponentString) grows items->items to any unknown id and allocates a
// placeholder Item there. Targets that reach it snapshot the table after start-up with
// flare_snapshot_item_table() and call flare_restore_item_table() after each input.
static void flare_fuzz_init_game() {
	camp = new CampaignManager();
	items = new ItemManager();
	powers = new PowerManager();
	loot = new LootManager();
	eventm = new EventManager();
}

// Replace the campaign with a fresh one, dropping every status an input registered. Nothing in
// the engine keeps a pointer to the CampaignManager other than the `camp` global, and statuses are
// referred to by hash only, so tables loaded at start-up still resolve (to "not set", as before).
static void flare_reset_campaign() {
	delete camp;
	camp = new CampaignManager();
}

// Item table layout after start-up: its size and which slots held no Item.
static size_t g_items_size;
static std::vector<bool> g_item_slot_empty;

static void flare_snapshot_item_table() {
	g_items_size = items->items.size();
	g_item_slot_empty.assign(g_items_size, false);
	for (size_t i = 0; i < g_items_size; ++i)
		g_item_slot_empty[i] = (items->items[i] == NULL);
}

// Delete every Item in a slot that was empty at the snapshot and shrink the table back to its
// snapshot size, so no input sees ids an earlier one allocated. Call it only once nothing built
// for the input (a Map, an NPC, a hero's inventory) still points at those slots.
static void flare_restore_item_table() {
	std::vector<Item*>& table = items->items;
	for (size_t i = 0; i < table.size(); ++i) {
		if (i >= g_items_size || g_item_slot_empty[i]) {
			delete table[i];
			table[i] = NULL;
		}
	}
	if (table.size() != g_items_size) {
		table.resize(g_items_size);
		table.shrink_to_fit();
	}
}

#endif
