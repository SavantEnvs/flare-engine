// Fuzz flare's loot table loader (LootManager: loot/*.txt, and parseLoot of a table reference).
#include "flare_fuzz_common.h"
#include "ItemManager.h"
#include "LootManager.h"
#include "PowerManager.h"

extern "C" int LLVMFuzzerInitialize(int*, char***) {
	flare_fuzz_init(FLARE_INIT_MEDIA);
	flare_fuzz_init_game();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	flare_write_mod_file("loot/fuzz.txt", data, size);
	LootManager* lm = new LootManager();
	// A map/enemy "loot=loot/fuzz.txt" reference expands the table into event components.
	std::string ref = "loot/fuzz.txt";
	EventComponent ec;
	std::vector<EventComponent> table;
	lm->parseLoot(ref, &ec, &table);
	delete lm;
	flare_reset_campaign();      // drop the statuses the input registered
	return 0;
}
