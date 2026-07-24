#include "o2/stdafx.h"
#include "UI/UIHelpers.h"

#include "o2/Assets/Types/AnimationAsset.h"
#include "o2/Scene/UI/WidgetLayer.h"

namespace UIHelpers
{
	void AddPressAnimation(const Ref<Button>& button, const char* layerName)
	{
		if (!button->FindLayer(layerName))
			return;

		if (!button->GetStateObject("hover"))
			button->AddState("hover", AssetRef<AnimationAsset>(kHoverAnimation))->offStateAnimationSpeed = 0.25f;

		if (!button->GetStateObject("pressed"))
			button->AddState("pressed", AssetRef<AnimationAsset>(kPressedAnimation))->offStateAnimationSpeed = 0.5f;
	}

	void AddPressAnimations(const Ref<Actor>& root)
	{
		if (!root)
			return;

		if (auto button = DynamicCast<Button>(root))
			AddPressAnimation(button);

		for (auto& child : root->GetChildren())
			AddPressAnimations(child);
	}
}
