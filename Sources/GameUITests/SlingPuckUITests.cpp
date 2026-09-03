#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "o2/Render/Render.h"
#include "o2/Render/Sprite.h"
#include "o2/Render/Text.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Components/SoundComponent.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/Widget.h"
#include "o2/Scene/UI/WidgetLayout.h"
#include "o2/Scene/UI/Widgets/Button.h"
#include "o2/Utils/Bitmap/Bitmap.h"
#include "o2/Utils/Serialization/DataValue.h"
#include "o2/Utils/Test/AppTestDriver.h"

#include "Jokes.h"
#include "Localization.h"
#include "SlingBackground.h"
#include "SlingBoard.h"
#include "SlingBot.h"
#include "SlingGameController.h"
#include "SlingGameFlow.h"
#include "SlingPuck.h"
#include "SlingPuckScene.h"

using namespace o2;

namespace
{
	const String kScreenshotsDir = "TestScreenshots/";

	// Counts distinct colors on a sparse grid; a real game frame has many, a blank frame one
	int CountDistinctColors(const Ref<Bitmap>& bitmap)
	{
		if (!bitmap)
			return 0;

		Vector<UInt32> seen;
		const UInt32* pixels = reinterpret_cast<const UInt32*>(bitmap->GetData());
		Vec2I size = bitmap->GetSize();
		for (int y = 0; y < size.y; y += 16)
		{
			for (int x = 0; x < size.x; x += 16)
			{
				UInt32 color = pixels[y * size.x + x];
				if (!seen.Contains(color))
					seen.Add(color);
			}
		}

		return seen.Count();
	}

	// Mean RGB over a sparse grid; the dialogs' dim layer must pull it down noticeably
	float AvgBrightness(const Ref<Bitmap>& bitmap)
	{
		if (!bitmap)
			return 0.0f;

		const UInt8* data = bitmap->GetData();
		Vec2I size = bitmap->GetSize();
		double sum = 0.0;
		int count = 0;
		for (int y = 0; y < size.y; y += 16)
		{
			for (int x = 0; x < size.x; x += 16)
			{
				const UInt8* pixel = data + (y * size.x + x) * 4;
				sum += (pixel[0] + pixel[1] + pixel[2]) / 3.0;
				count++;
			}
		}

		return count > 0 ? (float)(sum / count) : 0.0f;
	}
}

class SlingPuckUI: public ::testing::Test
{
protected:
	Ref<Actor>               root;
	Ref<SlingBoard>          board;
	Ref<SlingBot>            bot;
	Ref<SlingGameController> controller;
	Ref<SlingGameFlow>       flow;
	Ref<CameraActor>         camera;

	void SetUp() override
	{
		root = BuildSlingPuckScene();
		board = root->GetComponent<SlingBoard>();
		bot = root->GetComponent<SlingBot>();
		controller = root->GetComponent<SlingGameController>();
		flow = root->GetComponent<SlingGameFlow>();

		AppTestDriver::PumpFrames(5); // settle transforms, spawn the chips, prime the listeners layer

		bot->difficulty = 0.0f; // slowest bot, it won't interfere during the short test window
		                        // (set after the pump: the initial spawn resets it to startDifficulty)

		camera = o2Scene.GetCameras()[0].Lock();
		ASSERT_TRUE(camera);
	}

	void TearDown() override
	{
		if (camera)
			camera->Destroy();
		if (root)
			root->Destroy();

		AppTestDriver::PumpFrames(2); // let the scene flush destroyed actors
	}

	Vec2F WorldToScreen(const Vec2F& world) const
	{
		return camera->listenersLayer->ScreenFromLocal(world);
	}

	Vector<Ref<SlingPuck>> ActiveChipsOnSide(int side) const
	{
		Vector<Ref<SlingPuck>> chips;
		for (auto& puck : board->GetPucks())
		{
			if (puck && puck->active && SlingBoard::SideOfPosition(puck->position) == side)
				chips.Add(puck);
		}

		return chips;
	}

	// Spawns are random each round; parks the active chips at deterministic spots near the side
	// walls, keeping the centre column and the gap path free for scripted shots
	void ParkChipsAside()
	{
		for (int side = 0; side < 2; side++)
		{
			int slot = 0;
			for (auto& chip : ActiveChipsOnSide(side))
			{
				float y = 120.0f + 65.0f * (float)(slot / 2);
				chip->position = Vec2F(slot % 2 == 0 ? -180.0f : 180.0f, side == 0 ? -y : y);
				chip->velocity = Vec2F();
				slot++;
			}
		}

		AppTestDriver::PumpFrames(1); // sync the actors to the parked spots
	}

	// Mirrors every chip of the side onto the other half, so the controller declares its winner
	void ClearSide(int side)
	{
		for (auto& puck : ActiveChipsOnSide(side))
			puck->position.y = -puck->position.y;

		AppTestDriver::PumpFrames(3); // detect the win and pop the result window
	}

	// The result buttons' art is textless; the label is a "caption" Text layer that must fit
	// inside the layer's area (the button's inner capsule) and carry the outline font style
	void ExpectButtonCaptionFits(const Ref<Widget>& window, const String& buttonName)
	{
		auto button = DynamicCast<Widget>(window->GetChildWidget(buttonName));
		ASSERT_TRUE(button);
		auto caption = button->GetLayerDrawable<Text>("caption");
		ASSERT_TRUE(caption);
		EXPECT_FALSE(caption->GetText().IsEmpty());
		EXPECT_LE(caption->GetRealSize().x, caption->GetSize().x + 0.5f);
		EXPECT_LE(caption->GetRealSize().y, caption->GetSize().y + 0.5f);

		// The art's wooden frame and rounded caps take the outer part of the button: the text has
		// to stay well inside it, not run into the edge - it used to touch and overflow the capsule
		RectF buttonRect = button->layout->GetWorldRect();
		float textLeft = buttonRect.Center().x - caption->GetRealSize().x*0.5f;
		float textRight = buttonRect.Center().x + caption->GetRealSize().x*0.5f;
		EXPECT_GE(textLeft - buttonRect.left, buttonRect.Width()*0.1f);
		EXPECT_GE(buttonRect.right - textRight, buttonRect.Width()*0.1f);
		EXPECT_LE(caption->GetRealSize().y, buttonRect.Height()*0.65f);

		ASSERT_TRUE(caption->GetFontStyle());
		EXPECT_FALSE(caption->GetFontStyle()->GetEffects().IsEmpty()); // the stroke outline
	}

	void ClickWindowButton(const String& windowName, const String& buttonName)
	{
		auto window = root->GetChild(windowName);
		ASSERT_TRUE(window);
		auto button = DynamicCast<Widget>(window->GetChild(windowName + buttonName));
		ASSERT_TRUE(button);

		// a widget's worldPosition is its left-bottom corner; aim for the middle of its rect
		AppTestDriver::Click(WorldToScreen(button->layout->GetWorldRect().Center()));
		AppTestDriver::PumpFrames(2);
	}
};

TEST_F(SlingPuckUI, ScreenshotCapturesRealFrame)
{
	Ref<Bitmap> bitmap = AppTestDriver::TakeScreenshot();
	ASSERT_TRUE(bitmap);
	EXPECT_EQ(bitmap->GetSize(), o2Render.GetResolution());
	EXPECT_GT(CountDistinctColors(bitmap), 8); // wooden field, chips, bands — far from a blank frame

	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "capture_check.png"));
}

TEST_F(SlingPuckUI, PlayerDragsChipAndShootsThroughGap)
{
	// Park the random spawns aside and put one chip near the centre column: pulling it to the
	// band's middle (x = 0) gives a straight vertical launch through the gap onto the bot side
	ParkChipsAside();

	auto playerChips = ActiveChipsOnSide(0);
	ASSERT_FALSE(playerChips.IsEmpty());

	Ref<SlingPuck> chip = playerChips[0];
	chip->position = Vec2F(20.0f, -280.0f);
	AppTestDriver::PumpFrames(1);

	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "shot_1_initial.png"));

	Vec2F pullTarget(0.0f, -board->bottomHalfHeight - 60.0f); // cursor overshoots past the wall on purpose

	AppTestDriver::PressCursor(WorldToScreen(chip->position));
	EXPECT_TRUE(chip->held);

	AppTestDriver::MoveCursor(WorldToScreen(pullTarget), 15);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "shot_2_pulled.png")); // stretched band visible

	// the pull is limited by the walls: the chip (and so the band) never leaves the field
	EXPECT_GE(chip->position.y, -board->bottomHalfHeight + chip->radius - 0.5f);

	AppTestDriver::ReleaseCursor();
	EXPECT_FALSE(chip->held);

	// the flung band is heard right away
	auto shotSound = root->GetChild("BandShotSound")->GetComponent<SoundComponent>();
	ASSERT_TRUE(shotSound);
	EXPECT_TRUE(shotSound->IsPlaying());

	AppTestDriver::Wait(1.5f);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "shot_3_after.png"));

	EXPECT_GT(chip->position.y, 0.0f); // crossed to the bot side through the gap
}

TEST_F(SlingPuckUI, PlayerCannotUseBotBandOrCrossDivider)
{
	ParkChipsAside();

	// a chip on the bot's half is out of the player's reach: the press doesn't grab it
	auto botChips = ActiveChipsOnSide(1);
	ASSERT_FALSE(botChips.IsEmpty());
	auto botChip = botChips[0];
	Vec2F botChipPos = botChip->position;

	AppTestDriver::PressCursor(WorldToScreen(botChip->position));
	EXPECT_FALSE(botChip->held);
	AppTestDriver::MoveCursor(WorldToScreen(Vec2F(0.0f, 330.0f)), 8);
	AppTestDriver::ReleaseCursor();
	AppTestDriver::PumpFrames(1);
	EXPECT_LT((botChip->position - botChipPos).Length(), 1.0f);

	// dragging his own chip the player can't carry it across the divider onto the bot's half
	auto playerChips = ActiveChipsOnSide(0);
	ASSERT_FALSE(playerChips.IsEmpty());
	auto chip = playerChips[0];
	chip->position = Vec2F(20.0f, -120.0f);
	AppTestDriver::PumpFrames(1);

	AppTestDriver::PressCursor(WorldToScreen(chip->position));
	EXPECT_TRUE(chip->held);
	AppTestDriver::MoveCursor(WorldToScreen(Vec2F(0.0f, 200.0f)), 10);
	EXPECT_LE(chip->position.y, -chip->radius + 0.5f);

	AppTestDriver::ReleaseCursor(); // no backward stretch -> no shot, the chip just settles
	AppTestDriver::PumpFrames(1);
}

TEST_F(SlingPuckUI, BotPullsBandAndShoots)
{
	bot->difficulty = 100.0f; // fastest bot: fires 0.2 s after the game starts

	Vector<Vec2F> before;
	for (auto& puck : board->GetPucks())
		before.Add(puck->position);

	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "bot_1_initial.png"));

	int guard = 0;
	while (!bot->IsPulling() && guard++ < 300)
		AppTestDriver::PumpFrames(1);
	ASSERT_TRUE(bot->IsPulling());

	AppTestDriver::Wait(bot->pullDuration * 0.8f); // deep in the draw, the red band is stretched
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "bot_2_pulling.png"));

	AppTestDriver::Wait(1.5f); // release and flight
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "bot_3_after.png"));

	// the bot really shot something: a chip from its side moved substantially
	bool anyMoved = false;
	for (int i = 0; i < before.Count(); i++)
	{
		if (before[i].y > 0.0f && (board->GetPucks()[i]->position - before[i]).Length() > 50.0f)
			anyMoved = true;
	}
	EXPECT_TRUE(anyMoved);
}

TEST_F(SlingPuckUI, VictoryWindowShowsJokeOnDimAndNextLevelRaisesDifficulty)
{
	EXPECT_FLOAT_EQ(flow->GetDifficulty(), 10.0f); // the run starts at difficulty 10
	EXPECT_EQ(board->CountPucksOnSide(0), 3);      // with the minimum chips per side
	EXPECT_EQ(board->CountPucksOnSide(1), 3);

	float brightnessBefore = AvgBrightness(AppTestDriver::TakeScreenshot());

	ClearSide(0); // player side empty -> player won

	EXPECT_TRUE(flow->IsWindowShown());
	EXPECT_TRUE(root->GetChild("VictoryWindow")->IsEnabled());
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "window_victory.png"));

	// the dim layer darkens the whole frame behind the dialog
	float brightnessAfter = AvgBrightness(AppTestDriver::TakeScreenshot());
	EXPECT_LT(brightnessAfter, brightnessBefore * 0.95f);

	// the window carries a joke picked from the base, fitted into the plate area
	auto victoryWidget = DynamicCast<Widget>(root->GetChild("VictoryWindow"));
	ASSERT_TRUE(victoryWidget);
	auto joke = victoryWidget->GetLayerDrawable<Text>("joke");
	ASSERT_TRUE(joke);
	EXPECT_FALSE(joke->GetText().IsEmpty());
	EXPECT_LE(joke->GetRealSize().y, joke->GetSize().y + 0.5f);

	// even the longest joke of the base shrinks until it fits instead of spilling onto the button
	WString longest;
	for (int i = 0; i < Jokes::Count(); i++)
	{
		WString candidate((String)Jokes::At(i));
		if (candidate.Length() > longest.Length())
			longest = candidate;
	}
	joke->SetText(longest);
	SlingGameFlow::FitTextHeight(joke);
	EXPECT_LE(joke->GetRealSize().y, joke->GetSize().y + 0.5f);
	AppTestDriver::PumpFrames(1);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "window_victory_long_joke.png"));

	ExpectButtonCaptionFits(victoryWidget, "VictoryWindowNextButton");

	ClickWindowButton("VictoryWindow", "NextButton"); // NEXT LEVEL

	// the click is voiced
	auto clickSound = root->GetChild("ButtonClickSound")->GetComponent<SoundComponent>();
	ASSERT_TRUE(clickSound);
	EXPECT_TRUE(clickSound->IsPlaying());

	EXPECT_FALSE(root->GetChild("VictoryWindow")->IsEnabled());
	EXPECT_FLOAT_EQ(flow->GetDifficulty(), 20.0f);
	EXPECT_FLOAT_EQ(bot->difficulty, 20.0f);
	EXPECT_FALSE(controller->IsGameOver());
	EXPECT_EQ(board->CountPucksOnSide(0), 4); // difficulty 20 -> one more chip per side
	EXPECT_EQ(board->CountPucksOnSide(1), 4);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "window_next_level.png"));
}

TEST_F(SlingPuckUI, GameOverWindowRetryRestartsFromTen)
{
	flow->StartLevel(30.0f); // as if the player had climbed a few levels
	AppTestDriver::PumpFrames(1);
	EXPECT_EQ(board->CountPucksOnSide(0), 5); // difficulty 30 -> more chips on the field

	ClearSide(1); // bot side empty -> bot won, player lost

	EXPECT_TRUE(root->GetChild("GameOverWindow")->IsEnabled());
	EXPECT_FALSE(board->IsPlayerInputEnabled());

	// the window carries the red cross badge above the buttons
	auto gameOverWidget = DynamicCast<Widget>(root->GetChild("GameOverWindow"));
	ASSERT_TRUE(gameOverWidget);
	EXPECT_TRUE(gameOverWidget->GetLayer("cross"));

	ExpectButtonCaptionFits(gameOverWidget, "GameOverWindowRetryButton");
	ExpectButtonCaptionFits(gameOverWidget, "GameOverWindowWatchAdButton");

	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "window_gameover.png"));

	ClickWindowButton("GameOverWindow", "RetryButton"); // RETRY

	EXPECT_FALSE(root->GetChild("GameOverWindow")->IsEnabled());
	EXPECT_FLOAT_EQ(flow->GetDifficulty(), 10.0f);
	EXPECT_FLOAT_EQ(bot->difficulty, 10.0f);
	EXPECT_TRUE(board->IsPlayerInputEnabled());
	EXPECT_EQ(board->CountPucksOnSide(0), 3); // back to the starting count
	EXPECT_EQ(board->CountPucksOnSide(1), 3);
}

// The logo art was removed from field.png: the title is drawn as styled text over its banner
TEST_F(SlingPuckUI, TitleTextSitsOnTheFieldBanner)
{
	auto logo = DynamicCast<Widget>(root->GetChild("Logo"));
	ASSERT_TRUE(logo);

	auto first = logo->GetLayerDrawable<Text>("first");
	auto second = logo->GetLayerDrawable<Text>("second");
	ASSERT_TRUE(first);
	ASSERT_TRUE(second);
	EXPECT_EQ(first->GetText(), WString(Loc::Tr("слинг", "sling")));
	EXPECT_EQ(second->GetText(), WString(Loc::Tr("пак", "pluck")));

	// Each word is echoed by its outlined copy, so the two layers never drift apart
	EXPECT_EQ(logo->GetLayerDrawable<Text>("firstOutline")->GetText(), first->GetText());
	EXPECT_EQ(logo->GetLayerDrawable<Text>("secondOutline")->GetText(), second->GetText());

	// Both words carry a font style (gradient fill), and each has an outlined copy under it
	ASSERT_TRUE(first->GetFontStyle());
	EXPECT_FALSE(first->GetFontStyle()->GetEffects().IsEmpty());
	for (auto& outlineName : { "firstOutline", "secondOutline" })
	{
		auto outline = logo->GetLayerDrawable<Text>(outlineName);
		ASSERT_TRUE(outline);
		ASSERT_TRUE(outline->GetFontStyle());
		EXPECT_FALSE(outline->GetFontStyle()->GetEffects().IsEmpty());
	}

	// The words fit the banner box and follow each other without overlapping
	RectF logoRect = logo->layout->GetWorldRect();
	EXPECT_LE(first->GetRealSize().x + second->GetRealSize().x, logoRect.Width() + 0.5f);
	EXPECT_LE(Math::Max(first->GetRealSize().y, second->GetRealSize().y), logoRect.Height() + 0.5f);
	EXPECT_LT(first->GetRect().right, second->GetRect().left + 1.0f);

	// The banner is the top strip of the field art, above the play area
	EXPECT_GT(logoRect.bottom, board->topHalfHeight);

	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "logo_title.png"));
}

// The level counter lives at the other end of the banner and counts the player's wins from 1
TEST_F(SlingPuckUI, LevelLabelShowsAndAdvancesWithWins)
{
	auto label = DynamicCast<Widget>(root->GetChild("LevelLabel"));
	ASSERT_TRUE(label);

	auto text = label->GetLayerDrawable<Text>("caption");
	ASSERT_TRUE(text);
	EXPECT_EQ(text->GetText(), WString(Loc::Tr("Уровень: 1", "Level: 1")));

	// It sits in the banner, right of the title and inside the field
	RectF labelRect = label->layout->GetWorldRect();
	auto logo = DynamicCast<Widget>(root->GetChild("Logo"));
	ASSERT_TRUE(logo);
	EXPECT_GT(labelRect.left, logo->layout->GetWorldRect().right);
	EXPECT_GT(labelRect.bottom, board->topHalfHeight);
	EXPECT_LE(text->GetRealSize().x, labelRect.Width() + 0.5f);

	ClearSide(0); // the player cleared their half: a win
	ASSERT_TRUE(root->GetChild("VictoryWindow")->IsEnabled());

	ClickWindowButton("VictoryWindow", "NextButton");

	EXPECT_EQ(flow->GetLevel(), 2);
	EXPECT_EQ(text->GetText(), WString(Loc::Tr("Уровень: 2", "Level: 2")));
}

// Grass fills whatever the fitted camera shows beyond the field, at any window aspect
TEST_F(SlingPuckUI, GrassBackgroundCoversCameraView)
{
	auto background = root->GetChild("Background");
	ASSERT_TRUE(background);
	ASSERT_TRUE(background->GetComponent<SlingBackground>());

	AppTestDriver::PumpFrames(2); // the component sizes itself on update

	Vec2F view = SlingBackground::FittedViewSize(camera->GetFittedOrFixedSize(), o2Render.GetCurrentResolution());
	Vec2F size = background->transform->GetSize2D();
	EXPECT_GE(size.x, view.x);
	EXPECT_GE(size.y, view.y);

	// The camera margins are grass, not fill colour: sample the left edge column of the frame,
	// outside the 500-wide field, and check it is green
	Ref<Bitmap> bitmap = AppTestDriver::TakeScreenshot();
	ASSERT_TRUE(bitmap);

	Vec2I resolution = bitmap->GetSize();
	float fieldHalfInPixels = 250.0f / view.x * (float)resolution.x;
	int sampleX = Math::Max((int)((float)resolution.x * 0.5f - fieldHalfInPixels) / 2, 2);
	ASSERT_LT(sampleX * 2, resolution.x);

	const UInt8* data = bitmap->GetData();
	int greenPixels = 0, samples = 0;
	for (int y = resolution.y / 4; y < resolution.y * 3 / 4; y += 8)
	{
		const UInt8* pixel = data + (y * resolution.x + sampleX) * 4;
		if (pixel[1] > pixel[0] + 20 && pixel[1] > pixel[2] + 20)
			greenPixels++;
		samples++;
	}

	EXPECT_GT(greenPixels, samples * 3 / 4);
}

TEST_F(SlingPuckUI, WatchAdContinuesSameLevelAfterReward)
{
	flow->StartLevel(30.0f); // mid-run difficulty; WATCH AD must keep it, unlike RETRY
	AppTestDriver::PumpFrames(1);
	EXPECT_EQ(board->CountPucksOnSide(0), 5);

	ClearSide(1); // bot side empty -> player lost

	EXPECT_TRUE(root->GetChild("GameOverWindow")->IsEnabled());

	// off-wasm the SDK stub grants the reward right away; the flow consumes it on the next update
	ClickWindowButton("GameOverWindow", "WatchAdButton");
	AppTestDriver::PumpFrames(2);

	EXPECT_FALSE(root->GetChild("GameOverWindow")->IsEnabled());
	EXPECT_FLOAT_EQ(flow->GetDifficulty(), 30.0f); // continued, not restarted
	EXPECT_EQ(board->CountPucksOnSide(0), 5);
	EXPECT_TRUE(board->IsPlayerInputEnabled());
}

// The scene captions go through Loc: built under English they must carry the English texts
TEST(SlingPuckLocalization, EnglishSceneBuildsEnglishCaptions)
{
	struct LangGuard
	{
		Loc::Lang saved = Loc::GetLanguage();
		~LangGuard() { Loc::SetLanguage(saved); }
	} guard;

	Loc::SetLanguage(Loc::Lang::English);

	auto root = BuildSlingPuckScene();
	AppTestDriver::PumpFrames(2);

	auto window = DynamicCast<Widget>(root->GetChild("VictoryWindow"));
	ASSERT_TRUE(window);
	auto button = DynamicCast<Widget>(window->GetChildWidget("VictoryWindowNextButton"));
	ASSERT_TRUE(button);
	auto caption = button->GetLayerDrawable<Text>("caption");
	ASSERT_TRUE(caption);
	EXPECT_EQ(caption->GetText(), WString("NEXT LEVEL"));

	// The banner title goes through Loc as well
	auto logo = DynamicCast<Widget>(root->GetChild("Logo"));
	ASSERT_TRUE(logo);
	EXPECT_EQ(logo->GetLayerDrawable<Text>("first")->GetText(), WString("sling"));
	EXPECT_EQ(logo->GetLayerDrawable<Text>("second")->GetText(), WString("puck"));

	EXPECT_FALSE(Jokes::Random().IsEmpty());

	root = nullptr;
	o2Scene.Clear();
	o2Scene.UpdateDestroyingEntities();
	AppTestDriver::PumpFrames(1);
}

TEST_F(SlingPuckUI, PressedButtonSqueezesArtTogetherWithCaption)
{
	ClearSide(0); // player won -> the victory window with the NEXT LEVEL button

	auto window = DynamicCast<Widget>(root->GetChild("VictoryWindow"));
	ASSERT_TRUE(window);
	auto button = DynamicCast<Widget>(window->GetChildWidget("VictoryWindowNextButton"));
	ASSERT_TRUE(button);

	auto art = button->GetLayerDrawable<Sprite>("regular");
	auto caption = button->GetLayerDrawable<Text>("caption");
	ASSERT_TRUE(art);
	ASSERT_TRUE(caption);

	AppTestDriver::PressCursor(WorldToScreen(button->layout->GetWorldRect().Center()));
	AppTestDriver::Wait(0.3f); // the 0.06 s pressed animation completes with margin

	EXPECT_LT(art->GetScale2D().x, 0.9f);
	EXPECT_FLOAT_EQ(caption->GetScale2D().x, art->GetScale2D().x);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "button_pressed.png"));

	AppTestDriver::ReleaseCursor(); // the release lands the click and starts the next level
	AppTestDriver::PumpFrames(2);
	EXPECT_FALSE(root->GetChild("VictoryWindow")->IsEnabled());
}

// Reproduces the editor Game window: it loads the scene from the .scn asset instead of
// building it in code, so any button whose onClick was set as a lambda in
// BuildSlingPuckScene() comes back with an empty handler (lambdas don't serialize). The
// flow must (re)wire the result buttons on start, so clicking them works after a load too.
TEST(SlingPuckSerialized, ResultButtonsWorkAfterSceneReload)
{
	auto built = BuildSlingPuckScene();
	AppTestDriver::PumpFrames(2);

	DataDocument doc;
	o2Scene.Save(doc);

	built = nullptr;
	o2Scene.Clear();
	o2Scene.UpdateDestroyingEntities();
	AppTestDriver::PumpFrames(1);

	o2Scene.Load(doc);
	AppTestDriver::PumpFrames(5); // the reloaded flow's OnStart runs and wires the buttons

	Ref<Actor> root;
	Ref<SlingGameFlow> flow;
	for (auto& actor : o2Scene.GetRootActors())
	{
		if (auto f = actor->GetComponent<SlingGameFlow>())
		{
			root = actor;
			flow = f;
			break;
		}
	}
	ASSERT_TRUE(flow);

	auto window = root->GetChild("VictoryWindow");
	ASSERT_TRUE(window);
	auto nextButton = DynamicCast<Button>(window->GetChild("VictoryWindowNextButton"));
	ASSERT_TRUE(nextButton);

	EXPECT_FALSE(nextButton->onClick.IsEmpty()) << "result button lost its handler after load";

	auto caption = nextButton->GetLayerDrawable<Text>("caption");
	ASSERT_TRUE(caption);
	EXPECT_FALSE(caption->GetText().IsEmpty()) << "button caption lost after load";

	float difficultyBefore = flow->GetDifficulty();
	nextButton->onClick();
	EXPECT_GT(flow->GetDifficulty(), difficultyBefore) << "clicking NEXT LEVEL must raise difficulty";

	// The background keeps covering the view after a reload: its camera link must survive too
	auto background = root->GetChild("Background");
	ASSERT_TRUE(background);
	auto backgroundComponent = background->GetComponent<SlingBackground>();
	ASSERT_TRUE(backgroundComponent);
	ASSERT_TRUE(backgroundComponent->camera) << "background lost its camera link after load";

	AppTestDriver::PumpFrames(2);
	Vec2F view = SlingBackground::FittedViewSize(backgroundComponent->camera->GetFittedOrFixedSize(),
												 o2Render.GetCurrentResolution());
	EXPECT_GE(background->transform->GetSize2D().x, view.x);
	EXPECT_GE(background->transform->GetSize2D().y, view.y);

	// The title text and its styles survive as well
	auto logo = DynamicCast<Widget>(root->GetChild("Logo"));
	ASSERT_TRUE(logo);
	auto titleWord = logo->GetLayerDrawable<Text>("first");
	ASSERT_TRUE(titleWord);
	EXPECT_EQ(titleWord->GetText(), WString(Loc::Tr("слинг", "sling")));
	ASSERT_TRUE(titleWord->GetFontStyle());
	EXPECT_FALSE(titleWord->GetFontStyle()->GetEffects().IsEmpty());

	o2Scene.Clear();
	o2Scene.UpdateDestroyingEntities();
	AppTestDriver::PumpFrames(1);
}

TEST_F(SlingPuckUI, DragOutsideChipsDoesNothing)
{
	ParkChipsAside(); // the centre column is free, the drag below starts over empty wood

	Vector<Vec2F> before;
	for (auto& puck : board->GetPucks())
		before.Add(puck->position);

	AppTestDriver::Drag(WorldToScreen(Vec2F(0.0f, -60.0f)), WorldToScreen(Vec2F(0.0f, -430.0f)), 8);
	AppTestDriver::Wait(0.5f);

	for (int i = 0; i < before.Count(); i++)
		EXPECT_LT((board->GetPucks()[i]->position - before[i]).Length(), 1.0f);
}
