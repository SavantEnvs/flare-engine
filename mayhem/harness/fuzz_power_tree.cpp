// Fuzz flare's power tree loader (MenuPowers::loadPowerTree: powers/trees/*.txt — tabs, power
// cells, upgrades), the file a hero class names as its power_tree.
#include "flare_fuzz_common.h"
#include "Avatar.h"
#include "MapRenderer.h"
#include "MenuManager.h"
#include "MenuPowers.h"

extern "C" int LLVMFuzzerInitialize(int*, char***) {
	flare_fuzz_init(FLARE_INIT_MEDIA);
	// A power table for the tree to reference (ids 1..511; the game data defines up to ~400).
	std::string table;
	for (int id = 1; id < 512; ++id) {
		char buf[96];
		snprintf(buf, sizeof(buf), "[power]\nid=%d\nname=Power %d\ntype=fixed\n\n", id, id);
		table += buf;
	}
	flare_write_mod_text("powers/powers.txt", table);
	flare_fuzz_init_game();
	mapr = new MapRenderer();
	pc = new Avatar();
	// The menus need the text-input widget image, which only the game data ships.
	flare_install_placeholder_image("images/menus/input.png");
	// The action bar layout ships with the game data (fantasycore/menus/actionbar.txt); the
	// power menu assumes its 12 slots exist.
	flare_write_mod_text("menus/actionbar.txt",
		"pos=0,0,1280,70\nalign=bottom\n"
		"slot=1,64,6\nslot=2,128,6\nslot=3,192,6\nslot=4,256,6\nslot=5,320,6\nslot=6,384,6\n"
		"slot=7,448,6\nslot=8,512,6\nslot=9,576,6\nslot=10,640,6\nslot_M1=768,6\nslot_M2=832,6\n"
		"char_menu=960,6\ninv_menu=1024,6\npowers_menu=1088,6\nlog_menu=1152,6\n");
	menu = new MenuManager();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	flare_write_mod_file("powers/trees/fuzz.txt", data, size);
	MenuPowers* saved = menu_powers;
	MenuPowers* mp = new MenuPowers();
	mp->loadPowerTree("powers/trees/fuzz.txt");
	delete mp;
	menu_powers = saved;
	flare_reset_campaign();      // drop the statuses the input registered
	return 0;
}
