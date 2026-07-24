#pragma once

#include "o2/Scene/Actor.h"
#include "o2/Scene/UI/Widgets/Button.h"

using namespace o2;

namespace UIHelpers
{
	// Shared button state animation assets
	constexpr auto kPressedAnimation = "UI/ButtonPressed.anim";
	constexpr auto kHoverAnimation = "UI/ButtonHover.anim";

	// Adds hover fade and pressed squeeze states from the shared animation
	// assets, animating the button layer; no-op when the layer is missing
	// (headless or custom-built buttons)
	void AddPressAnimation(const Ref<Button>& button, const char* layerName = "regular");

	// Adds press animations to every button under the root
	void AddPressAnimations(const Ref<Actor>& root);
}
