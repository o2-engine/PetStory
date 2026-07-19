#include "o2/stdafx.h"
#include "ChipLitDemoScene.h"

#include "o2/Assets/Types/MaterialAsset.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Scene/Components/ScriptableComponent.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/WidgetLayout.h"
#include "o2/Scene/UI/Widgets/Label.h"

namespace
{
	struct ChipDef
	{
		const char* name;
		const char* albedo;
		const char* material;
		const char* reference;
		Vec2F       size;
		float       spinSpeed; // radians per second
	};

	const ChipDef kChips[] = {
		{ "red",  "ChipLit/red_df.png",      "ChipLit/red_sdf.mat",  "Game field/Objects/Main/red.png",  Vec2F(210, 210),  0.7f },
		{ "blue", "ChipLit/blue_df.png",     "ChipLit/blue_sdf.mat", "Game field/Objects/Main/blue.png", Vec2F(210, 210), -1.1f },
		{ "leaf", "ChipLit/leaf_albedo.png", "ChipLit/leaf_lit.mat", "Game field/Objects/leaf.png",      Vec2F(198, 315),  1.7f },
	};

	Ref<Actor> MakeSpinningLitChip(const ChipDef& chip, const Vec2F& position)
	{
		auto actor = mmake<Actor>(ActorCreateMode::InScene);
		actor->SetName(String("lit ") + chip.name);
		actor->transform->SetPivot2D(Vec2F(0.5f, 0.5f));
		actor->transform->SetSize2D(chip.size);
		actor->transform->SetPosition2D(position);

		auto image = mmake<ImageComponent>(chip.albedo);
		image->SetMaterialAsset(AssetRef<MaterialAsset>(chip.material));
		actor->AddComponent(image);

		auto spin = mmake<ScriptableComponent>();
		spin->SetScript(AssetRef<JavaScriptAsset>("Scripts/ChipSpin.js"));
		actor->AddComponent(spin);
		spin->GetInstance().SetProperty("speed", chip.spinSpeed);

		return actor;
	}

	Ref<Actor> MakeStaticRefChip(const ChipDef& chip, const Vec2F& position)
	{
		auto actor = mmake<Actor>(ActorCreateMode::InScene);
		actor->SetName(String("ref ") + chip.name);
		actor->transform->SetPivot2D(Vec2F(0.5f, 0.5f));
		actor->transform->SetSize2D(chip.size);
		actor->transform->SetPosition2D(position);
		actor->AddComponent(mmake<ImageComponent>(chip.reference));
		return actor;
	}

	Ref<Label> MakeLabel(const String& name, const WString& text, const Vec2F& center, const Vec2F& size)
	{
		auto label = mmake<Label>();
		label->SetName(name);
		label->SetFontAsset(AssetRef<FontAsset>("debugFont.ttf"));
		label->SetText(text);
		label->SetHorAlign(HorAlign::Middle);
		label->SetVerAlign(VerAlign::Middle);

		label->layout->anchorMin = Vec2F(0.5f, 0.5f);
		label->layout->anchorMax = Vec2F(0.5f, 0.5f);
		label->layout->offsetMin = center - size*0.5f;
		label->layout->offsetMax = center + size*0.5f;

		return label;
	}
}

Ref<CameraActor> BuildChipLitDemoScene()
{
	auto camera = mmake<CameraActor>();
	camera->SetName("chip lit demo camera");
	camera->fillColor = Color4(26, 26, 30);
	camera->SetFittedSize(Vec2F(1280, 1024));
	camera->AddToScene();

	const float rowY[] = { 320.0f, 0.0f, -330.0f };
	for (int i = 0; i < 3; i++)
	{
		MakeSpinningLitChip(kChips[i], Vec2F(-280.0f, rowY[i]));
		MakeStaticRefChip(kChips[i], Vec2F(280.0f, rowY[i]));
	}

	MakeLabel("lit label", "chip_lit shader (spinning)", Vec2F(-280, 480), Vec2F(500, 30));
	MakeLabel("ref label", "reference (static)", Vec2F(280, 480), Vec2F(500, 30));

	return camera;
}
