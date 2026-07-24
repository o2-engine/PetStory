#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "Level/LevelController.h"
#include "Data/UserDataModel.h"
#include "Level/LevelChain.h"
#include "Screens/GameplayScreen.h"
#include "Screens/MetaScreen.h"
#include "GameLib/Localization/Localization.h"
#include "GameLib/Screens/ScreenManager.h"
#include "GameLib/Windows/WindowManager.h"
#include "Windows/BuyMovesWindow.h"
#include "Windows/SettingsWindow.h"
#include "Windows/WinWindow.h"
#include "o2/Animation/AnimationClip.h"
#include "o2/Application/Application.h"
#include "o2/Render/Render.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Components/ScriptableComponent.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/Widgets/Button.h"
#include "o2/Scene/UI/Widgets/Label.h"
#include "o2/Utils/Bitmap/Bitmap.h"
#include "o2/Utils/FileSystem/FileSystem.h"
#include "o2/Utils/Test/AppTestDriver.h"

using namespace o2;

namespace
{
	const String kScreenshotsDir = "TestScreenshots/";
}

// Windows over the real screens: prototypes instantiate, the JS window logic
// binds the buttons and drives the flow through the injected action callback
class WindowsUI: public ::testing::Test
{
protected:
	Ref<ScreenManager> screens;
	Ref<WindowManager> windows;

	void SetUp() override
	{
		UserDataModel::Reset();
		LevelChain::Reset();
		ASSERT_TRUE(LevelChain::Load());
		ASSERT_TRUE(Localization::LoadLanguage("Localization/ru.json"));

		windows = mmake<WindowManager>();
		windows->AddWindow(mmake<SettingsWindow>());
		windows->AddWindow(mmake<WinWindow>());
		windows->AddWindow(mmake<BuyMovesWindow>());

		screens = mmake<ScreenManager>();
		screens->AddScreen(mmake<MetaScreen>());
		screens->AddScreen(mmake<GameplayScreen>());
		screens->ShowScreen(MetaScreen::kName);

		PumpManager(3);
	}

	void TearDown() override
	{
		screens->Clear();
		screens = nullptr;
		windows->Clear();
		windows = nullptr;
		UserDataModel::Reset();
		LevelChain::Reset();
		Localization::Reset();

		o2Scene.Clear(true);
		o2Scene.UpdateDestroyingEntities();
		AppTestDriver::PumpFrames(2);
	}

	void PumpManager(int frames)
	{
		for (int i = 0; i < frames; i++)
		{
			screens->Update(o2Time.GetDeltaTime());
			AppTestDriver::PumpFrames(1);
		}
	}

	void PumpManagerTime(float seconds)
	{
		float time = 0.0f;
		while (time < seconds)
		{
			float dt = Math::Max(o2Time.GetDeltaTime(), 1.0f/60.0f);
			screens->Update(dt);
			AppTestDriver::PumpFrames(1);
			time += dt;
		}
	}

	Vec2F WorldToScreen(const Vec2F& world) const
	{
		auto camera = o2Scene.GetCameras()[0].Lock();
		return camera->listenersLayer->ScreenFromLocal(world);
	}

	// Widgets keep the pivot at the rect corner, so aim at the rect center
	void ClickChild(const Ref<Actor>& root, const String& name)
	{
		auto child = root->FindChild(name);
		ASSERT_TRUE(child) << "no child " << name.Data();
		AppTestDriver::Click(WorldToScreen(child->transform->GetWorldRect().Center()));
	}

	void SaveShot(const String& name)
	{
		o2FileSystem.FolderCreate(kScreenshotsDir, true);
		AppTestDriver::SaveScreenshot(kScreenshotsDir + name);
	}
};

TEST_F(WindowsUI, SettingsWindowOpensAndClosesByClicks)
{
	auto metaScreen = DynamicCast<MetaScreen>(screens->GetCurrentScreen());
	ASSERT_TRUE(metaScreen);

	// The settings button on the meta screen opens the window
	ClickChild(metaScreen->GetRoot(), "SettingsButton");
	PumpManager(3);

	auto window = windows->GetWindow(SettingsWindow::kName);
	ASSERT_TRUE(window);
	EXPECT_TRUE(window->IsShown());

	auto root = window->GetRoot();
	ASSERT_TRUE(root);
	EXPECT_TRUE(root->FindChild("Panel"));
	EXPECT_TRUE(root->FindChild("SoundToggle"));
	EXPECT_TRUE(root->FindChild("MusicToggle"));
	EXPECT_TRUE(root->FindChild("OkButton"));

	// The prototype carries the JS window logic
	auto script = root->GetComponent<ScriptableComponent>();
	ASSERT_TRUE(script);
	EXPECT_TRUE(script->GetInstance().IsObject());

	SaveShot("settings_window.png");

	// Texts come from the localization table, not baked images
	auto headTitle = DynamicCast<Label>(root->FindChild("HeadTitle"));
	ASSERT_TRUE(headTitle);
	EXPECT_EQ(headTitle->GetText(), Localization::GetText("settings.title"));

	// The JS toggle handler drives the animated "value" state and the user data
	auto toggle = DynamicCast<Widget>(root->FindChild("SoundToggle"));
	ASSERT_TRUE(toggle);
	EXPECT_TRUE(toggle->GetState("value"));

	EXPECT_TRUE(UserDataModel::Get().soundEnabled);
	ClickChild(root, "SoundToggle");
	PumpManager(2);
	EXPECT_FALSE(UserDataModel::Get().soundEnabled);
	EXPECT_FALSE(toggle->GetState("value"));

	// The toggle state animation slides the knob to the off side
	AppTestDriver::PumpFrames(60);
	auto knob = DynamicCast<Widget>(toggle->FindChild("Knob"));
	ASSERT_TRUE(knob);
	EXPECT_LT(knob->transform->GetWorldRect().Center().x, toggle->transform->GetWorldRect().Center().x);

	SaveShot("settings_window_toggled.png");

	// Close through the JS-bound close button
	ClickChild(root, "CloseButton");
	PumpManager(2);
	EXPECT_FALSE(window->IsShown());
}

TEST_F(WindowsUI, ButtonPressSqueezesFace)
{
	auto metaScreen = DynamicCast<MetaScreen>(screens->GetCurrentScreen());
	ASSERT_TRUE(metaScreen);

	auto playButton = DynamicCast<Button>(metaScreen->GetRoot()->FindChild("PlayButton"));
	ASSERT_TRUE(playButton);

	SaveShot("play_button_rest.png");
	Ref<Bitmap> before = AppTestDriver::TakeScreenshot();
	ASSERT_TRUE(before);

	// Hold the cursor down: the shared pressed animation squeezes the whole
	// button, caption included, around its center
	Vec2F screenPos = WorldToScreen(playButton->transform->GetWorldRect().Center());
	AppTestDriver::MoveCursor(screenPos);
	AppTestDriver::PressCursor(screenPos);
	AppTestDriver::PumpFrames(60);

	SaveShot("play_button_pressed.png");
	Ref<Bitmap> pressed = AppTestDriver::TakeScreenshot();
	ASSERT_TRUE(pressed);

	// The press must actually change pixels around the button, not just the
	// transform property
	RectF worldRect = playButton->transform->GetWorldRect();
	Vec2F screenCenter = (Vec2F)o2Render.GetResolution()*0.5f;
	Vec2F leftBottom = WorldToScreen(worldRect.LeftBottom());
	Vec2F rightTop = WorldToScreen(worldRect.RightTop());

	// Bitmap rows are bottom-up, matching the y-up screen space
	int changed = 0;
	for (int y = (int)(screenCenter.y + leftBottom.y); y < (int)(screenCenter.y + rightTop.y); y++)
	{
		for (int x = (int)(leftBottom.x + screenCenter.x); x < (int)(rightTop.x + screenCenter.x); x++)
		{
			const UInt8* a = before->GetData() + (y*before->GetSize().x + x)*4;
			const UInt8* b = pressed->GetData() + (y*pressed->GetSize().x + x)*4;
			if (Math::Abs((int)a[0] - (int)b[0]) > 16 || Math::Abs((int)a[1] - (int)b[1]) > 16 ||
				Math::Abs((int)a[2] - (int)b[2]) > 16)
				changed++;
		}
	}
	EXPECT_GT(changed, 200) << "pressed button must visibly squeeze";

	AppTestDriver::ReleaseCursor();
	AppTestDriver::PumpFrames(60);
}

TEST_F(WindowsUI, WinWindowShowsStarsAndAdvancesLevel)
{
	screens->ShowScreen(GameplayScreen::kName);
	PumpManager(3);

	auto gameplay = DynamicCast<GameplayScreen>(screens->GetCurrentScreen());
	ASSERT_TRUE(gameplay);

	int levelBefore = UserDataModel::Get().currentLevel;

	auto controller = gameplay->GetLevelController();
	ASSERT_TRUE(controller);
	for (auto& goal : controller->GetGoals())
		controller->OnChipsPopped(goal.chipType, goal.count);

	PumpManagerTime(1.5f); // the win window opens after the completion delay

	auto window = windows->GetWindow(WinWindow::kName);
	ASSERT_TRUE(window);
	ASSERT_TRUE(window->IsShown());

	auto root = window->GetRoot();
	ASSERT_TRUE(root);

	// One pop of the whole goal leaves nearly all moves: full three stars
	EXPECT_TRUE(root->FindChild("StarGold1")->IsEnabled());
	EXPECT_TRUE(root->FindChild("StarGold3")->IsEnabled());
	EXPECT_FALSE(root->FindChild("StarBlue3")->IsEnabled());

	// The title and the next caption come from the localization table
	auto title = DynamicCast<Label>(root->FindChild("Title"));
	ASSERT_TRUE(title);
	EXPECT_EQ(title->GetText(), Localization::GetText("win.title"));

	auto nextCaption = DynamicCast<Label>(root->FindChild("NextButton")->FindChild("Caption"));
	ASSERT_TRUE(nextCaption);
	EXPECT_EQ(nextCaption->GetText(), Localization::GetText("win.next"));

	SaveShot("win_window.png");

	ClickChild(root, "NextButton");
	PumpManager(3);

	EXPECT_FALSE(window->IsShown());
	EXPECT_EQ(screens->GetCurrentScreen()->GetName(), MetaScreen::kName);
	EXPECT_EQ(UserDataModel::Get().currentLevel, levelBefore + 1);
}

TEST_F(WindowsUI, BuyMovesWindowBuysMovesByClick)
{
	screens->ShowScreen(GameplayScreen::kName);
	PumpManager(3);

	auto gameplay = DynamicCast<GameplayScreen>(screens->GetCurrentScreen());
	ASSERT_TRUE(gameplay);

	auto controller = gameplay->GetLevelController();
	ASSERT_TRUE(controller);
	ASSERT_TRUE(controller->HasMovesLimit());

	// The moves HUD shows the remaining count
	auto movesLabel = DynamicCast<Label>(gameplay->GetRoot()->FindChild("MovesLabel"));
	ASSERT_TRUE(movesLabel);
	EXPECT_EQ(movesLabel->GetText(), (WString)(String)controller->GetMovesLeft());

	while (controller->GetMovesLeft() > 0)
		controller->OnChipsPopped("NoSuchColor", 1);

	PumpManager(3);

	auto window = windows->GetWindow(BuyMovesWindow::kName);
	ASSERT_TRUE(window);
	ASSERT_TRUE(window->IsShown());

	auto root = window->GetRoot();
	ASSERT_TRUE(root);

	// JS fills the coins HUD label from the injected balance
	auto coinsLabel = DynamicCast<Label>(root->FindChild("CoinsLabel"));
	ASSERT_TRUE(coinsLabel);
	EXPECT_EQ(coinsLabel->GetText(), (WString)(String)UserDataModel::Get().coins);

	// The offer text is the localized template with the real price
	auto offerLabel = DynamicCast<Label>(root->FindChild("OfferLabel"));
	ASSERT_TRUE(offerLabel);
	EXPECT_EQ(offerLabel->GetText(), Localization::Format("buyMoves.offer", {
		{ "moves", (WString)(String)BuyMovesWindow::kMoves },
		{ "price", (WString)(String)BuyMovesWindow::kPrice } }));

	SaveShot("buy_moves_window.png");

	int coinsBefore = UserDataModel::Get().coins;
	ClickChild(root, "BuyButton");
	PumpManager(2);

	EXPECT_FALSE(window->IsShown());
	EXPECT_EQ(UserDataModel::Get().coins, coinsBefore - BuyMovesWindow::kPrice);
	EXPECT_EQ(controller->GetMovesLeft(), BuyMovesWindow::kMoves);
	EXPECT_EQ(movesLabel->GetText(), (WString)(String)BuyMovesWindow::kMoves);
}

TEST_F(WindowsUI, MetaScreenShowsHudAndWindowsUnloadOnSwitch)
{
	auto metaScreen = DynamicCast<MetaScreen>(screens->GetCurrentScreen());
	ASSERT_TRUE(metaScreen);

	auto root = metaScreen->GetRoot();
	EXPECT_TRUE(root->FindChild("LivesHeart"));
	EXPECT_TRUE(root->FindChild("CoinsBack"));
	EXPECT_TRUE(root->FindChild("PlusButton"));
	EXPECT_TRUE(root->FindChild("FbButton"));

	auto livesLabel = DynamicCast<Label>(root->FindChild("LivesLabel"));
	ASSERT_TRUE(livesLabel);
	EXPECT_EQ(livesLabel->GetText(), (WString)(String)UserDataModel::Get().lives);

	auto coinsLabel = DynamicCast<Label>(root->FindChild("CoinsLabel"));
	ASSERT_TRUE(coinsLabel);
	EXPECT_EQ(coinsLabel->GetText(), (WString)(String)UserDataModel::Get().coins);

	SaveShot("meta_screen_hud.png");

	// A shown window unloads when the screen switches
	auto window = windows->ShowWindow(SettingsWindow::kName);
	ASSERT_TRUE(window);
	AppTestDriver::PumpFrames(2);

	screens->ShowScreen(GameplayScreen::kName);
	PumpManager(3);

	EXPECT_FALSE(window->IsLoaded());
}
