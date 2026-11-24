// Fuzz flare's NPC definition loader (NPC::load: npcs/*.txt — portraits, vendor stock, dialog
// trees and dialog groups) and the dialog-tree selection the talker menu runs on it.
// The hero, map renderer and menus are created once and only read. Each input's NPC is deleted after
// the dialog selection. Its dialog events can name reward_item=<id> in the old syntax, which makes
// EventManager::loadEventComponentString call ItemManager::verifyID(..., VERIFY_ALLOCATE): that grows
// the global item table to the id and allocates a placeholder Item. After each input the harness
// restores the item table to its start-up layout (flare_restore_item_table), so no input sees ids an
// earlier one allocated.
#include "flare_fuzz_common.h"
#include "Avatar.h"
#include "Entity.h"
#include "MapRenderer.h"
#include "MenuManager.h"
#include "NPC.h"

extern "C" int LLVMFuzzerInitialize(int*, char***) {
	flare_fuzz_init(FLARE_INIT_MEDIA);
	flare_fuzz_init_game();
	mapr = new MapRenderer();
	pc = new Avatar();
	// Dialog requirements check the hero's inventory (CampaignManager::checkItem/checkCurrency).
	// The menus need the text-input widget image, which only the game data ships.
	flare_install_placeholder_image("images/menus/input.png");
	menu = new MenuManager();
	flare_snapshot_item_table();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	flare_write_mod_file("npcs/fuzz.txt", data, size);
	NPC* npc = new NPC(Entity());
	if (npc->load("npcs/fuzz.txt")) {
		// What MenuTalker does when the player starts talking: pick the available dialog
		// nodes (one per dialog group) and read their topics.
		std::vector<int> nodes;
		npc->getDialogNodes(nodes, true);
		volatile size_t n = 0;
		for (size_t i = 0; i < nodes.size(); ++i)
			n += npc->getDialogTopic(static_cast<unsigned>(nodes[i])).size();
	}
	delete npc;
	flare_restore_item_table(); // drop the placeholder Items reward_item events allocated
	flare_reset_campaign();      // drop the statuses the input registered
	return 0;
}
