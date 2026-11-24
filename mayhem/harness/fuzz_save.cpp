// Fuzz flare's save-game loader (SaveLoad::loadGame: saves/<save_prefix>/<slot>/avatar.txt in
// the user dir). Save files are shared between players, so they are untrusted input; the loader
// reads the hero's stats, equipment/carried item ids and quantities, action bar and power ids,
// campaign statuses, spawn map and stash tab, and indexes the item and power tables with them.
//
// Per input the hero state is rebuilt from scratch, as GameStatePlay does for a new game:
// the CampaignManager, MapRenderer, Avatar, MenuManager (inventory, stash,
// vendor buyback, action bar, power menu) and ModManager (its location cache sees the input's
// spawn map name) are deleted and re-created. The power, loot and engine tables are read only.
// The item table is not: ItemStorage::setItems calls ItemManager::verifyID(..., VERIFY_ALLOCATE),
// which grows items->items to any unknown id and allocates a placeholder Item there. After each
// input the harness deletes every Item in a slot that was empty at start-up and shrinks the
// table back to its start-up size, so no input sees ids an earlier one allocated.
#include "flare_fuzz_common.h"
#include "Avatar.h"
#include "MapRenderer.h"
#include "MenuManager.h"

static std::vector<std::string> g_mod_list(1, "fuzz");
static std::string g_save_path;
extern "C" int LLVMFuzzerInitialize(int*, char***) {
	flare_fuzz_init(FLARE_INIT_MEDIA);
	// Item and power tables for the save file to reference (the game data defines up to a few
	// thousand items and ~400 powers; mods/default ships neither).
	std::string table;
	for (int id = 1; id < 512; ++id) {
		char buf[96];
		snprintf(buf, sizeof(buf), "[power]\nid=%d\nname=Power %d\ntype=fixed\n\n", id, id);
		table += buf;
	}
	flare_write_mod_text("powers/powers.txt", table);
	table.clear();
	for (int id = 1; id < 256; ++id) {
		char buf[96];
		snprintf(buf, sizeof(buf), "[item]\nid=%d\nname=Item %d\nmax_quantity=%d\n\n", id, id, id % 2 ? 1 : 99);
		table += buf;
	}
	flare_write_mod_text("items/items.txt", table);
	flare_fuzz_init_game();
	// Fixtures only the game data ships (see fuzz_power_tree.cpp).
	flare_install_placeholder_image("images/menus/input.png");
	flare_write_mod_text("menus/actionbar.txt",
		"pos=0,0,1280,70\nalign=bottom\n"
		"slot=1,64,6\nslot=2,128,6\nslot=3,192,6\nslot=4,256,6\nslot=5,320,6\nslot=6,384,6\n"
		"slot=7,448,6\nslot=8,512,6\nslot=9,576,6\nslot=10,640,6\nslot_M1=768,6\nslot_M2=832,6\n"
		"char_menu=960,6\ninv_menu=1024,6\npowers_menu=1088,6\nlog_menu=1152,6\n");
	save_load->setGameSlot(1);
	flare_snapshot_item_table();
	g_save_path = settings->path_user + "saves/" + eset->misc.save_prefix + "/1/avatar.txt";
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	// Fresh hero state for this input, torn down again at the end. The Avatar is deleted only
	// after loadGame() ran (which loads its animations): ~Entity releases them and
	// AnimationManager::decreaseCount exits on a name it never loaded.
	delete mods;
	mods = new ModManager(&g_mod_list);
	// A new CampaignManager rather than resetAllStatuses(), which only disables statuses and
	// would keep every status name an input registered.
	flare_reset_campaign();
	mapr = new MapRenderer();
	pc = new Avatar();
	menu = new MenuManager();

	flare_write_file(g_save_path, data, size);
	save_load->loadGame();

	delete menu;
	delete pc;
	delete mapr;
	menu = NULL;
	pc = NULL;
	mapr = NULL;
	anim->cleanUp(); // drop the animation sets this input's gfx names loaded
	flare_restore_item_table(); // drop the placeholder Items verifyID allocated for unknown ids
	return 0;
}
