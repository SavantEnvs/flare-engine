// Fuzz flare's animation definition loader (AnimationSet::load via getAnimation: animations/*.txt) and the
// frame set-up of each animation it defines.
#include "flare_fuzz_common.h"
#include "Animation.h"
#include "AnimationSet.h"

extern "C" int LLVMFuzzerInitialize(int*, char***) {
	flare_fuzz_init(FLARE_INIT_MEDIA);
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	flare_write_mod_file("animations/fuzz.txt", data, size);
	AnimationSet* set = new AnimationSet("animations/fuzz.txt");
	delete set->getAnimation(""); // the first lookup parses the definition file
	// Instantiate each defined animation (and the default one) and step it, as an entity
	// does when it is drawn.
	std::vector<std::string> names(1, "");
	for (size_t i = 0; i < set->animations.size() && i < 32; ++i)
		names.push_back(set->animations[i]->getName());
	for (size_t i = 0; i < names.size(); ++i) {
		Animation* a = set->getAnimation(names[i]);
		for (int f = 0; f < 8; ++f) a->advanceFrame();
		a->getCurrentFrame(0);
		delete a;
	}
	delete set;
	return 0;
}
