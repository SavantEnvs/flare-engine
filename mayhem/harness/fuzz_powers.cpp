// Fuzz flare's power/effect definitions loader (PowerManager: powers/powers.txt).
#include "flare_fuzz_common.h"
#include "ItemManager.h"
#include "PowerManager.h"

extern "C" int LLVMFuzzerInitialize(int*, char***) {
	flare_fuzz_init(FLARE_INIT_MEDIA);
	flare_fuzz_init_game();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	flare_write_mod_file("powers/powers.txt", data, size);
	PowerManager* pm = new PowerManager();
	delete pm;
	flare_reset_campaign();      // drop the statuses the input registered
	return 0;
}
