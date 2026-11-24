// Fuzz flare's creature definition loader (StatBlock::load: enemies/*.txt).
#include "flare_fuzz_common.h"
#include "PowerManager.h"
#include "StatBlock.h"

extern "C" int LLVMFuzzerInitialize(int*, char***) {
	flare_fuzz_init(FLARE_INIT_MEDIA);
	flare_fuzz_init_game();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	flare_write_mod_file("enemies/fuzz.txt", data, size);
	StatBlock* s = new StatBlock();
	s->load("enemies/fuzz.txt");
	s->recalc();
	delete s;
	flare_reset_campaign();      // drop the statuses the input registered
	return 0;
}
