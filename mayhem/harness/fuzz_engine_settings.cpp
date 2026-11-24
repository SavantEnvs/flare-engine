// Fuzz flare's engine configuration loaders (EngineSettings::load: engine/*.txt).
// The first input byte selects which engine/<name>.txt the rest of the input replaces;
// the other files come from mods/default.
#include "flare_fuzz_common.h"

static const char* const kEngineFiles[] = {
	"classes", "combat", "damage_types", "death_penalty", "elements", "equip_flags",
	"gameplay", "loot", "misc", "number_format", "primary_stats", "resolutions",
	"resource_stats", "tileset_config", "tooltips", "widget_settings", "xp_table",
};
static const size_t kNumEngineFiles = sizeof(kEngineFiles) / sizeof(kEngineFiles[0]);
static std::vector<std::string> g_mod_list(1, "fuzz");

extern "C" int LLVMFuzzerInitialize(int*, char***) {
	flare_fuzz_init(FLARE_INIT_CORE);
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	if (size < 1) return -1;
	// Drop the previous input's file and the mod manager's location cache, so only this
	// input's file overrides mods/default.
	for (size_t i = 0; i < kNumEngineFiles; ++i)
		unlink((g_fuzz_mod + "engine/" + kEngineFiles[i] + ".txt").c_str());
	std::string name = std::string("engine/") + kEngineFiles[data[0] % kNumEngineFiles] + ".txt";
	flare_write_mod_file(name, data + 1, size - 1);
	delete mods;
	mods = new ModManager(&g_mod_list);

	EngineSettings* e = new EngineSettings();
	EngineSettings* saved = eset;
	eset = e; // sub-loaders read other sub-settings through the global (e.g. loot -> misc)
	e->load();
	eset = saved;
	delete e;
	return 0;
}
