// Fuzz flare's gettext .po reader (GetText) through MessageEngine, which loads every
// languages/engine.<lang>.po and languages/data.<lang>.po of the active mods.
#include "flare_fuzz_common.h"

static void install_language() {
	// Register a language that no shipped mod translates, so only the fuzz input is parsed.
	flare_write_mod_text("engine/languages.txt", "APPEND\nzz=Fuzz\n");
}

extern "C" int LLVMFuzzerInitialize(int*, char***) {
	flare_fuzz_init(FLARE_INIT_CORE, install_language);
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	flare_write_mod_file("languages/engine.zz.po", data, size);
	settings->language = "zz";
	MessageEngine* m = new MessageEngine();
	// Look up a few engine keys so the translated (unescaped) values are used, as the menus do.
	volatile size_t n = 0;
	n += m->get("Inventory").size();
	n += m->get("Character").size();
	n += m->get("Level %d").size();
	delete m;
	return 0;
}
