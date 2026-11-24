// Fuzz flare's two user-editable metadata readers. The first input byte selects:
//   even: the player's settings.txt (Settings::loadSettings, read from the config dir)
//   odd:  a mod's settings.txt (ModManager::loadMod: name, version, engine version, depends)
#include "flare_fuzz_common.h"
#include "Version.h"

extern "C" int LLVMFuzzerInitialize(int*, char***) {
	flare_fuzz_init(FLARE_INIT_CORE);
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	if (size < 1) return -1;
	if ((data[0] & 1) == 0) {
		flare_write_file(settings->path_conf + "settings.txt", data + 1, size - 1);
		// loadSettings() first resets every option to its default, so no state carries over.
		settings->loadSettings();
		volatile size_t n = settings->language.size() + settings->render_device_name.size();
		(void)n;
	}
	else {
		flare_write_mod_file("settings.txt", data + 1, size - 1);
		Mod m = mods->loadMod("fuzz");
		volatile size_t n = m.name.size() + m.depends.size();
		(void)n;
		std::string v = m.version->getString();
	}
	return 0;
}
