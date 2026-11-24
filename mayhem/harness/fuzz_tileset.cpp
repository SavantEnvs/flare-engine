// Fuzz flare's tileset definition loader (TileSet::load: tilesetdefs/*.txt).
#include "flare_fuzz_common.h"
#include "TileSet.h"

extern "C" int LLVMFuzzerInitialize(int*, char***) {
	flare_fuzz_init(FLARE_INIT_MEDIA);
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	flare_write_mod_file("tilesetdefs/fuzz.txt", data, size);
	TileSet* t = new TileSet();
	t->load("tilesetdefs/fuzz.txt");
	t->logic(); // advance tile animations once, as the map renderer does every frame
	delete t;
	return 0;
}
