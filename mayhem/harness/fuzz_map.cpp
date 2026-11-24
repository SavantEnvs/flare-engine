// Fuzz flare's map loader (Map::load: maps/*.txt — header, layers, enemy groups, NPCs, events).
// The hero and map renderer are created once and only read. Each input's Map is deleted after the
// load. Its [event] sections can name reward_item=<id> in the old syntax, which makes
// EventManager::loadEventComponentString call ItemManager::verifyID(..., VERIFY_ALLOCATE): that grows
// the global item table to the id and allocates a placeholder Item. After each input the harness
// restores the item table to its start-up layout (flare_restore_item_table), so no input sees ids an
// earlier one allocated.
#include "flare_fuzz_common.h"
#include "Avatar.h"
#include "Map.h"
#include "MapRenderer.h"

extern "C" int LLVMFuzzerInitialize(int*, char***) {
	flare_fuzz_init(FLARE_INIT_MEDIA);
	flare_fuzz_init_game();
	// Event stat blocks tick against the hero, whose set-up reads the active map renderer.
	mapr = new MapRenderer();
	pc = new Avatar();
	flare_snapshot_item_table();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	flare_write_mod_file("maps/fuzz.txt", data, size);
	Map* m = new Map();
	m->load("maps/fuzz.txt", false);
	delete m;
	flare_restore_item_table(); // drop the placeholder Items reward_item events allocated
	flare_reset_campaign();      // drop the statuses the input registered
	return 0;
}
