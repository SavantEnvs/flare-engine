// Known-answer self-test of flare's data loaders (built with upstream's normal flags).
// Each check loads a committed fixture through the engine's own loader and asserts the parsed
// values; prints "PASS <name>" / "FAIL <name>" per check, which mayhem/test.sh counts.
#include "flare_fuzz_common.h"
#include "ItemManager.h"
#include "PowerManager.h"

#include <fstream>
#include <sstream>

static int g_failed = 0;
static void check(const char* name, bool ok) {
	printf("%s %s\n", ok ? "PASS" : "FAIL", name);
	if (!ok) ++g_failed;
}

static std::string read_file(const std::string& path) {
	std::ifstream f(path.c_str(), std::ios::binary);
	std::stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

static std::string g_fixtures;

static void install_language() {
	flare_write_mod_text("engine/languages.txt", "APPEND\nzz=Selftest\n");
	flare_write_mod_text("languages/engine.zz.po", read_file(g_fixtures + "/languages/engine.zz.po"));
	flare_write_mod_text("items/items.txt", read_file(g_fixtures + "/items/items.txt"));
}

int main(int argc, char** argv) {
	if (argc != 2) { fprintf(stderr, "usage: %s <fixtures dir>\n", argv[0]); return 2; }
	g_fixtures = argv[1];
	flare_fuzz_init(FLARE_INIT_MEDIA, install_language);

	// Engine settings from mods/default/engine/*.txt
	check("engine_primary_stats_count", eset->primary_stats.list.size() == 4);
	check("engine_primary_stat_ids", eset->primary_stats.list.size() == 4 &&
	      eset->primary_stats.list[0].id == "physical" && eset->primary_stats.list[3].id == "defense");
	check("engine_damage_types_loaded", eset->damage_types.list.size() >= 3 && eset->damage_types.list[0].id == "melee");

	// gettext .po through MessageEngine
	settings->language = "zz";
	MessageEngine* m = new MessageEngine();
	check("po_translation", m->get("Inventory") == "Selftest Inventory");
	check("po_fuzzy_ignored", m->get("Character") == "Character");
	check("po_multiline", m->get("Multi") == "Line one line two");
	check("po_untranslated_passthrough", m->get("No such key") == "No such key");
	delete m;

	// Item database
	camp = new CampaignManager();
	powers = new PowerManager();
	ItemManager* im = new ItemManager();
	items = im;
	check("items_sword_valid", im->isValid(1));
	check("items_gap_invalid", !im->isValid(2));
	check("items_sword_name", im->isValid(1) && im->getItemName(1) == "Selftest Sword");
	check("items_sword_price", im->isValid(1) && static_cast<int>(im->items[1]->price.base) == 123);
	check("items_potion_stack", im->isValid(5) && im->items[5]->max_quantity == 20);
	check("items_potion_level", im->isValid(5) && im->items[5]->level == 2);

	printf("selftest: %d failed\n", g_failed);
	return g_failed ? 1 : 0;
}
