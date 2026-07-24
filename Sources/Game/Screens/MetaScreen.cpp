#include "o2/stdafx.h"
#include "Screens/MetaScreen.h"

#include "Data/UserDataModel.h"
#include "Level/LevelChain.h"
#include "Screens/GameplayScreen.h"
#include "GameLib/Screens/ScreenManager.h"
#include "Windows/SettingsWindow.h"
#include "UI/UIHelpers.h"
#include "GameLib/Windows/WindowManager.h"
#include "o2/Render/Render.h"
#include "o2/Render/Sprite.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/WidgetLayer.h"
#include "o2/Scene/UI/WidgetLayout.h"
#include "o2/Scene/UI/Widgets/Image.h"
#include "o2/Scene/UI/Widgets/Label.h"

namespace
{
	const Vec2F kDesignSize(2160.0f, 3840.0f);
	const char* kFont = "Fonts/GrilledCheese BTN.ttf";

	// Image and font loading is skipped without the render device (headless
	// tests): actors and logic stay, only the visuals are dropped
	Ref<Actor> MakeSprite(const String& name, const Vec2F& pos, const Vec2F& size, const String& imagePath)
	{
		auto actor = mmake<Actor>(ActorCreateMode::InScene);
		actor->SetName(name);
		actor->transform->SetPivot2D(Vec2F(0.5f, 0.5f));
		actor->transform->SetSize2D(size);
		actor->transform->SetPosition2D(pos);

		if (Render::IsSingletonInitialzed())
			actor->AddComponent(mmake<ImageComponent>(imagePath));

		return actor;
	}

	// pos is the widget center relative to the anchor point of the parent
	void SetLayout(const Ref<Widget>& widget, const Vec2F& anchor, const Vec2F& pos, const Vec2F& size)
	{
		widget->layout->anchorMin = anchor;
		widget->layout->anchorMax = anchor;
		widget->layout->offsetMin = pos - size * 0.5f;
		widget->layout->offsetMax = pos + size * 0.5f;
	}

	Ref<Image> MakeImage(const String& name, const String& imagePath,
						 const Vec2F& anchor, const Vec2F& pos, const Vec2F& size)
	{
		auto image = mmake<Image>();
		image->SetName(name);

		if (Render::IsSingletonInitialzed())
			image->SetImageAsset(AssetRef<ImageAsset>(imagePath));

		SetLayout(image, anchor, pos, size);
		return image;
	}

	Ref<Label> MakeLabel(const String& name, const WString& text,
						 const Vec2F& anchor, const Vec2F& pos, const Vec2F& size, int height)
	{
		// Even the Label constructor loads a font, so no labels at all without render
		if (!Render::IsSingletonInitialzed())
			return nullptr;

		auto label = mmake<Label>();
		label->SetName(name);
		label->SetFontAsset(AssetRef<FontAsset>(kFont));
		label->SetText(text);
		label->SetHeight(height);
		label->SetHorAlign(HorAlign::Middle);
		label->SetVerAlign(VerAlign::Middle);
		SetLayout(label, anchor, pos, size);
		return label;
	}

	// Button with the frame back layer and the pressable face on top. The face
	// stretches with pixel insets so the shared press animation squeezes it
	// through the layer anchors; face-only buttons fill the whole widget
	Ref<Button> MakeButton(const String& name, const String& backPath, const String& facePath,
						   const Vec2F& faceSize, const Vec2F& anchor, const Vec2F& pos, const Vec2F& size)
	{
		auto button = mmake<Button>();
		button->SetName(name);

		if (Render::IsSingletonInitialzed())
		{
			if (!backPath.IsEmpty())
				button->AddLayer("back", mmake<Sprite>(backPath), Layout::BothStretch());

			Vec2F inset = backPath.IsEmpty() ? Vec2F() : (size - faceSize) * 0.5f;
			button->AddLayer("regular", mmake<Sprite>(facePath),
							 Layout::BothStretch(inset.x, inset.y, inset.x, inset.y));

			UIHelpers::AddPressAnimation(button);
		}

		SetLayout(button, anchor, pos, size);

		// The press animation scales the button around its center
		button->layout->SetPivot2D(Vec2F(0.5f, 0.5f));

		return button;
	}
}

String MetaScreen::GetName() const
{
	return kName;
}

const Ref<Actor>& MetaScreen::GetRoot() const
{
	return mRoot;
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
	if (auto shadowImage = shadow->GetComponent<ImageComponent>())
		shadowImage->SetTransparency(0.4f);
	shadow->SetParent(mRoot);

	MakeSprite("Dog", Vec2F(0.0f, -320.0f), Vec2F(1067.0f, 1644.0f), "Animals/Dog/dog_normal.png")->SetParent(mRoot);

	// HUD root spans the whole design canvas; HUD elements anchor to its
	// corners and edges, offsets keep the PSD pixel positions
	auto ui = mmake<Widget>();
	ui->SetName("UIRoot");
	SetLayout(ui, Vec2F(0.5f, 0.5f), Vec2F(), kDesignSize);
	ui->SetParent(mRoot);

	BuildLivesPanel(ui);
	BuildCoinsPanel(ui);
	BuildButtons(ui);
}

void MetaScreen::BuildLivesPanel(const Ref<Widget>& ui)
{
	const Vec2F topLeft(0.0f, 1.0f);

	ui->AddChildWidget(MakeImage("LivesBack", "Animal screen/HeartsBg.png",
								 topLeft, Vec2F(288.0f, -237.0f), Vec2F(375.0f, 361.0f)));
	ui->AddChildWidget(MakeImage("LivesHeart", "Animal screen/Heart.png",
								 topLeft, Vec2F(282.0f, -176.0f), Vec2F(336.0f, 241.0f)));

	if (auto lives = MakeLabel("LivesLabel", (WString)(String)UserDataModel::Get().lives,
							   topLeft, Vec2F(276.0f, -182.0f), Vec2F(180.0f, 150.0f), 110))
		ui->AddChildWidget(lives);

	if (auto timer = MakeLabel("LivesTimer", "12:34",
							   topLeft, Vec2F(292.0f, -338.0f), Vec2F(240.0f, 90.0f), 58))
		ui->AddChildWidget(timer);
}

void MetaScreen::BuildCoinsPanel(const Ref<Widget>& ui)
{
	const Vec2F topRight(1.0f, 1.0f);

	ui->AddChildWidget(MakeImage("CoinsBack", "Animal screen/CoinsBg.png",
								 topRight, Vec2F(-497.0f, -195.0f), Vec2F(657.0f, 184.0f)));
	ui->AddChildWidget(MakeImage("CoinsIcon", "Animal screen/Coin.png",
								 topRight, Vec2F(-202.0f, -186.0f), Vec2F(220.0f, 217.0f)));

	if (auto coins = MakeLabel("CoinsLabel", (WString)(String)UserDataModel::Get().coins,
							   topRight, Vec2F(-510.0f, -194.0f), Vec2F(330.0f, 100.0f), 64))
		ui->AddChildWidget(coins);

	// Sits on the dark circle at the left end of the coins plank
	auto plusButton = MakeButton("PlusButton", "", "Animal screen/PlusButton.png", Vec2F(),
								 topRight, Vec2F(-738.0f, -195.0f), Vec2F(170.0f, 165.0f));
	ui->AddChildWidget(plusButton);
}

void MetaScreen::BuildButtons(const Ref<Widget>& ui)
{
	auto settingsButton = MakeButton("SettingsButton", "Animal screen/SettingsBg.png",
									 "Animal screen/SettingsButton.png", Vec2F(304.0f, 280.0f),
									 Vec2F(0.0f, 0.0f), Vec2F(263.0f, 254.0f), Vec2F(348.0f, 320.0f));
	settingsButton->onClick = [] { MetaScreen::OpenSettings(); };
	ui->AddChildWidget(settingsButton);

	auto playButton = MakeButton("PlayButton", "Animal screen/PlayBg.png",
								 "Animal screen/PlayBtn.png", Vec2F(405.0f, 357.0f),
								 Vec2F(0.5f, 0.0f), Vec2F(20.0f, 333.0f), Vec2F(473.0f, 414.0f));
	playButton->onClick = [] {
		if (auto manager = ScreenManager::Instance())
			manager->ShowScreen(GameplayScreen::kName);
	};
	ui->AddChildWidget(playButton);

	auto fbButton = MakeButton("FbButton", "Animal screen/FbBg.png",
							   "Animal screen/GbButton.png", Vec2F(304.0f, 279.0f),
							   Vec2F(1.0f, 0.0f), Vec2F(-267.0f, 254.0f), Vec2F(348.0f, 320.0f));
	ui->AddChildWidget(fbButton);
}

void MetaScreen::OpenSettings()
{
	auto windows = WindowManager::Instance();
	if (!windows)
		return;

	auto window = windows->GetWindow(SettingsWindow::kName);
	if (!window)
		return;

	window->Load();
	window->SetScriptProperty("soundOn", UserDataModel::Get().soundEnabled);
	window->SetScriptProperty("musicOn", UserDataModel::Get().musicEnabled);

	window->onAction = [](const String& action) {
		if (action == "close")
		{
			if (auto windows = WindowManager::Instance())
				windows->HideWindow(SettingsWindow::kName);
		}
		else if (action == "soundOn")
			UserDataModel::SetSoundEnabled(true);
		else if (action == "soundOff")
			UserDataModel::SetSoundEnabled(false);
		else if (action == "musicOn")
			UserDataModel::SetMusicEnabled(true);
		else if (action == "musicOff")
			UserDataModel::SetMusicEnabled(false);
	};

	window->Show();
}

void MetaScreen::OnUnload()
{
	if (auto windows = WindowManager::Instance())
		windows->UnloadAll();

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
