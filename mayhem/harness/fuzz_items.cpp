// Fuzz flare's item database loader (ItemManager: items/items.txt).
#include "flare_fuzz_common.h"
#include "ItemManager.h"
#include "PowerManager.h"

extern "C" int LLVMFuzzerInitialize(int*, char***) {
	flare_fuzz_init(FLARE_INIT_MEDIA);
	flare_fuzz_init_game();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	flare_write_mod_file("items/items.txt", data, size);
	ItemManager* im = new ItemManager();
	ItemManager* saved = items;
	items = im;
	// Build each item's display name, as the inventory/tooltips do.
	volatile size_t n = 0;
	for (size_t id = 1; id < im->items.size() && id < 4096; ++id) {
		if (im->isValid(static_cast<ItemID>(id)))
			n += im->getItemName(static_cast<ItemID>(id)).size();
	}
	items = saved;
	delete im;
	flare_reset_campaign();      // drop the statuses the input registered
	return 0;
}
