#include "o2/stdafx.h"
#include "Screens/MetaScreen.h"

#include "Screens/GameplayScreen.h"
#include "Screens/ScreenManager.h"
#include "o2/Animation/AnimationClip.h"
#include "o2/Render/Sprite.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/WidgetLayer.h"
#include "o2/Scene/UI/WidgetLayout.h"
#include "o2/Scene/UI/Widgets/Button.h"

namespace
{
	const Vec2F kDesignSize(2160.0f, 3840.0f);

	Ref<Actor> MakeSprite(const String& name, const Vec2F& pos, const Vec2F& size, const String& imagePath)
	{
		auto actor = mmake<Actor>(ActorCreateMode::InScene);
		actor->SetName(name);
		actor->transform->SetPivot2D(Vec2F(0.5f, 0.5f));
		actor->transform->SetSize2D(size);
		actor->transform->SetPosition2D(pos);
		actor->AddComponent(mmake<ImageComponent>(imagePath));
		return actor;
	}
}

void MetaScreen::OnLoad()
{
	mRoot = mmake<Actor>(ActorCreateMode::InScene);
	mRoot->SetName("MetaScreen");
	mRoot->transform->SetPosition2D(Vec2F());

	auto camera = mmake<CameraActor>();
	camera->SetName("Camera");
	camera->fillColor = Color4(43, 24, 20);
	camera->SetFittedSize(kDesignSize);
	camera->SetParent(mRoot);
	camera->transform->SetPosition2D(Vec2F());

	MakeSprite("Back", Vec2F(), kDesignSize, "Animal screen/Back.png")->SetParent(mRoot);

	auto shadow = MakeSprite("DogShadow", Vec2F(0.0f, -1140.0f), Vec2F(1238.0f, 246.0f), "Animals/Dog/shadow.png");
	shadow->GetComponent<ImageComponent>()->SetTransparency(0.4f);
	shadow->SetParent(mRoot);

	MakeSprite("Dog", Vec2F(0.0f, -320.0f), Vec2F(1067.0f, 1644.0f), "Animals/Dog/dog_normal.png")->SetParent(mRoot);

	// Play button: brown plate layer with the red play button on top, pressing squeezes both
	const Vec2F buttonSize(700.0f, 613.0f);
	const Vec2F buttonPos(0.0f, -1580.0f);
	const float innerScale = 0.8f;

	auto playButton = mmake<Button>();
	playButton->SetName("PlayButton");
	playButton->AddLayer("back", mmake<Sprite>("Animal screen/PlayBg.png"), Layout::BothStretch());

	Vec2F innerSize = buttonSize * innerScale;
	playButton->AddLayer("regular", mmake<Sprite>("Animal screen/PlayBtn.png"),
						 Layout(Vec2F(0.5f, 0.5f), Vec2F(0.5f, 0.5f), innerSize * -0.5f, innerSize * 0.5f));

	playButton->AddState("hover", AnimationClip::EaseInOut("layer/regular/transparency", 1.0f, 0.85f, 0.1f))
		->offStateAnimationSpeed = 0.25f;

	playButton->AddState("pressed", AnimationClip::EaseInOut("layer/regular/mDrawable/scale",
															 Vec2F(1.0f, 1.0f), Vec2F(0.88f, 0.88f), 0.06f))
		->offStateAnimationSpeed = 0.5f;

	playButton->layout->anchorMin = Vec2F(0.5f, 0.5f);
	playButton->layout->anchorMax = Vec2F(0.5f, 0.5f);
	playButton->layout->offsetMin = buttonPos - buttonSize * 0.5f;
	playButton->layout->offsetMax = buttonPos + buttonSize * 0.5f;

	playButton->onClick = [] {
		if (auto manager = ScreenManager::Instance())
			manager->ShowScreen(GameplayScreen::kName);
	};

	playButton->SetParent(mRoot);

	o2Scene.UpdateAddedEntities();
	o2Scene.UpdateTransforms();
}

void MetaScreen::OnUnload()
{
	if (mRoot)
	{
		mRoot->SetEnabled(false);
		o2Scene.DestroyActor(mRoot);
		mRoot = nullptr;
	}
}

void MetaScreen::OnActivated()
{
	if (mRoot)
		mRoot->SetEnabled(true);
}

void MetaScreen::OnDeactivated()
{
	if (mRoot)
		mRoot->SetEnabled(false);
}
