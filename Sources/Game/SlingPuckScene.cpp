#include "o2/stdafx.h"
#include "SlingPuckScene.h"

#include "o2/Animation/AnimationClip.h"
#include "o2/Render/Sprite.h"
#include "o2/Render/Text.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Scene/Components/SoundComponent.h"
#include "o2/Scene/UI/Widget.h"
#include "o2/Scene/UI/WidgetLayer.h"
#include "o2/Scene/UI/WidgetLayout.h"
#include "o2/Scene/UI/Widgets/Button.h"
#include "o2/Utils/Debug/Debug.h"
#include "o2/Utils/Math/Color.h"

#include "SlingBoard.h"
#include "SlingBot.h"
#include "SlingGameController.h"
#include "SlingGameFlow.h"
#include "SlingPuck.h"
#include "SlingRubber.h"

static Ref<Actor> MakeSprite(const String& name, const Vec2F& pos, const Vec2F& size, const String& imagePath)
{
	auto actor = mmake<Actor>(ActorCreateMode::InScene);
	actor->SetName(name);
	actor->transform->SetPivot2D(Vec2F(0.5f, 0.5f));
	actor->transform->SetSize2D(size);
	actor->transform->SetPosition2D(pos);
	actor->AddComponent(mmake<ImageComponent>(imagePath));
	return actor;
}

// Sprite button in the BasicUIStyle manner: a "regular" layer with the art, pressing squeezes the
// art through the "pressed" state animation, hovering lights it up slightly
static Ref<Button> MakeSpriteButton(const String& name, const String& imagePath,
									const Vec2F& center, const Vec2F& size)
{
	auto button = mmake<Button>();
	button->SetName(name);
	button->AddLayer("regular", mmake<Sprite>(imagePath), Layout::BothStretch());

	button->AddState("hover", AnimationClip::EaseInOut("layer/regular/transparency", 1.0f, 0.85f, 0.1f))
		->offStateAnimationSpeed = 0.25f;

	button->AddState("pressed", AnimationClip::EaseInOut("layer/regular/mDrawable/scale",
														 Vec2F(1.0f, 1.0f), Vec2F(0.88f, 0.88f), 0.06f))
		->offStateAnimationSpeed = 0.5f;

	button->layout->anchorMin = Vec2F(0.5f, 0.5f);
	button->layout->anchorMax = Vec2F(0.5f, 0.5f);
	button->layout->offsetMin = center - size * 0.5f;
	button->layout->offsetMax = center + size * 0.5f;

	return button;
}

// Result dialog root: an invisible widget with a full-screen darkening layer, positioned by
// centre/size in field space; panel art, text and buttons are layered on top by the callers
static Ref<Widget> MakeDimWindow(const String& name, const Vec2F& center, const Vec2F& size)
{
	auto window = mmake<Widget>(ActorCreateMode::InScene);
	window->SetName(name);
	// The dim expands far past the window so it covers the whole visible field at any window aspect.
	// Alpha lives on the layer: the layer overwrites its drawable's transparency with its own.
	auto dim = window->AddLayer("dim", mmake<Sprite>(Color4(0, 0, 0, 255)),
								Layout::BothStretch(-900.0f, -900.0f, -900.0f, -900.0f), -1.0f);
	dim->transparency = 0.6f;
	window->layout->anchorMin = Vec2F(0.5f, 0.5f);
	window->layout->anchorMax = Vec2F(0.5f, 0.5f);
	window->layout->offsetMin = center - size * 0.5f;
	window->layout->offsetMax = center + size * 0.5f;
	return window;
}

Ref<Actor> BuildSlingPuckScene()
{
	// field.png 768x1376 (banner + frame + divider are baked in), drawn at 500x896 world
	// (scale 0.6512). The baked divider bar (image rows 701..737) sits below the image centre,
	// so the whole field is shifted up by fieldOffsetY to land its middle at world y = 0.
	const float fieldW = 500.0f;
	const float fieldH = 896.0f;
	const float fieldOffsetY = 20.2f;

	// Physics play area aligned to the inner wood of field.png: side walls at image x 25/740,
	// top wall at image y 135, bottom wall at y 1343 — the halves are not the same depth
	const float playHalfW = 232.0f;
	const float playTopH = 380.0f;
	const float playBottomH = 406.0f;
	const float gapHalf = 34.0f; // half-width of the notch in the divider (image x 332..436 scaled)
	const float radius = 30.0f;  // chips in the reference screens are ~92 px -> 60 world units

	// Fitted camera targets exactly the field size, so the field fills the view and touches
	// the camera edges by height or width, whichever the window aspect hits first
	auto camera = mmake<CameraActor>();
	camera->SetName("Camera");
	camera->fillColor = Color4(24, 26, 34);
	camera->SetFittedSize(Vec2F(fieldW, fieldH));
	camera->AddToScene();
	camera->transform->SetPosition2D(Vec2F(0.0f, fieldOffsetY));

	auto root = mmake<Actor>(ActorCreateMode::InScene);
	root->SetName("SlingGame");
	root->transform->SetPosition2D(Vec2F(0.0f, 0.0f));

	auto board = root->AddComponent<SlingBoard>();
	auto bot = root->AddComponent<SlingBot>();
	auto controller = root->AddComponent<SlingGameController>();
	auto flow = root->AddComponent<SlingGameFlow>();

	board->halfWidth = playHalfW;
	board->topHalfHeight = playTopH;
	board->bottomHalfHeight = playBottomH;
	board->gapHalf = gapHalf;
	board->friction = 1.2f;

	bot->board.Set(board.Get());
	bot->difficulty = flow->startDifficulty;
	controller->board.Set(board.Get());
	controller->bot.Set(bot.Get());
	flow->board.Set(board.Get());
	flow->bot.Set(bot.Get());
	flow->controller.Set(controller.Get());

	MakeSprite("Field", Vec2F(0.0f, fieldOffsetY), Vec2F(fieldW, fieldH), "field.png")->SetParent(root);

	// One-shot sounds (CC0, see Sounds/Sounds.license.txt); each is restarted on every event
	auto makeSound = [&](const String& name, const String& assetPath) {
		auto actor = mmake<Actor>(ActorCreateMode::InScene);
		actor->SetName(name);
		auto sound = actor->AddComponent<SoundComponent>();
		sound->SetSound(AssetRef<SoundAsset>(assetPath));
		actor->SetParent(root);
		return sound;
	};

	auto chipHitSound1 = makeSound("ChipHitSound1", "Sounds/chip_collide1.ogg");
	auto chipHitSound2 = makeSound("ChipHitSound2", "Sounds/chip_collide2.ogg");
	auto bandShotSound = makeSound("BandShotSound", "Sounds/band_shot.ogg");
	auto buttonClickSound = makeSound("ButtonClickSound", "Sounds/button_click.ogg");

	// Chip hits: audible above a light tap, volume follows the impact speed, alternating samples
	// with a pitch jitter so streaks of collisions don't sound machine-like
	WeakRef<SoundComponent> weakHit1(chipHitSound1.Get());
	WeakRef<SoundComponent> weakHit2(chipHitSound2.Get());
	board->onPuckHit = [weakHit1, weakHit2](float impactSpeed) {
		if (impactSpeed < 60.0f)
			return;

		auto sound = (Math::Random(0.0f, 1.0f) < 0.5f ? weakHit1 : weakHit2).Lock();
		if (!sound)
			return;

		sound->SetVolume(0.2f + 0.8f * Math::Clamp01(impactSpeed / 1200.0f));
		sound->SetPitch(Math::Random(0.9f, 1.1f));
		sound->RewindAndPlay();
	};

	// Rubber bands near the back edges (the same wall insets as in the reference screens),
	// stretched onto the side walls (posts at image x 14/753)
	const float bandTopY = 322.0f;
	const float bandBottomY = -338.0f;
	const float bandSpan = 240.0f;
	const float bandThickness = 20.0f;

	WeakRef<SoundComponent> weakShot(bandShotSound.Get());
	auto makeRubber = [&](int side, const Color4& bandColor) {
		auto rubberActor = mmake<Actor>(ActorCreateMode::InScene);
		rubberActor->SetName(side == 0 ? "RubberPlayer" : "RubberBot");
		rubberActor->transform->SetPosition2D(Vec2F(0.0f, 0.0f));
		auto rubber = rubberActor->AddComponent<SlingRubber>();
		rubber->side = side;
		rubber->restY = side == 0 ? bandBottomY : bandTopY;
		rubber->halfSpan = bandSpan;
		rubber->thickness = bandThickness;
		rubber->color = bandColor;
		rubber->onShot = [weakShot](float launchSpeed) {
			if (auto sound = weakShot.Lock())
			{
				sound->SetVolume(0.4f + 0.6f * Math::Clamp01(launchSpeed / 1700.0f));
				sound->SetPitch(Math::Random(0.95f, 1.05f));
				sound->RewindAndPlay();
			}
		};
		rubberActor->SetParent(root);
	};

	makeRubber(0, Color4(235, 130, 60, 255)); // player band, warm red-orange (bottom)
	makeRubber(1, Color4(70, 110, 165, 255)); // bot band, blue (top)

	// Chip pool: the flow activates 3..10 per side depending on difficulty and randomizes their
	// spots each round, so the pool holds the maximum for both sides. Grid positions here are
	// placeholders, replaced on the first flow update. Colours cycle (0 = blue, 1 = red, 2 = green).
	// Each chip is a root actor with a soft contact shadow under the art: Shadow is added first so
	// it draws below Body, and the highlight rotation spins only Body, keeping the shadow still.
	auto makeChipChild = [](const Ref<Actor>& parent, const String& name, const Vec2F& localPos,
							const Vec2F& size, const String& imagePath) {
		auto child = mmake<Actor>(ActorCreateMode::InScene);
		child->SetName(name);
		child->transform->SetPivot2D(Vec2F(0.5f, 0.5f));
		child->SetParent(parent);
		child->transform->SetSize2D(size);
		child->transform->SetPosition2D(localPos);
		auto image = mmake<ImageComponent>(imagePath);
		child->AddComponent(image);
		return image;
	};

	for (int i = 0; i < flow->maxPucksPerSide * 2; i++)
	{
		int side = i % 2;
		int slot = i / 2;
		int color = i % 3;

		Vec2F pos(-160.0f + (slot % 5) * 80.0f, (120.0f + (slot / 5) * 90.0f) * (side == 0 ? -1.0f : 1.0f));
		String image = color == 0 ? "blue_chip.png" : (color == 1 ? "red_chip.png" : "green_chip.png");

		auto actor = mmake<Actor>(ActorCreateMode::InScene);
		actor->SetName("Chip");
		actor->transform->SetPivot2D(Vec2F(0.5f, 0.5f));
		actor->transform->SetSize2D(Vec2F(radius * 2.0f, radius * 2.0f));
		actor->transform->SetPosition2D(pos);

		auto shadow = makeChipChild(actor, "Shadow", Vec2F(3.0f, -5.0f),
									Vec2F(radius * 2.3f, radius * 2.3f), "shadow.png");
		shadow->SetColor(Color4(30, 30, 30, 130)); // darker and semi-transparent

		makeChipChild(actor, "Body", Vec2F(0.0f, 0.0f), Vec2F(radius * 2.0f, radius * 2.0f), image);

		auto puck = actor->AddComponent<SlingPuck>();
		puck->team = color;
		puck->radius = radius;
		puck->dragPower = 40.0f; // shallow pulls (the band stays inside the field) still shoot hard
		puck->position = pos;

		actor->SetParent(root);
	}

	// Divider cap, added last so it draws above the chips and they slide under it through the gap.
	// Sized and placed so the solid bar of separator.png (rows 270-354, cols 35-1669 of 1687x624)
	// lands exactly on the divider baked into field.png (world y in ±11.7, x in ±240)
	MakeSprite("Separator", Vec2F(-2.5f, 0.0f), Vec2F(495.0f, 174.0f), "separator.png")->SetParent(root);

	// Result dialogs, after everything else so they draw on top; the flow shows one per round end.
	// Geometry comes from the reference screens victory_screen.png / gameover.png (768x1376).
	WeakRef<SlingGameFlow> weakFlow(flow.Get());
	const Vec2F resultButtonSize(277.0f, 86.0f);

	// Victory: the victory.png panel (crown + plate) with the joke inside and one wide button
	auto victoryWindow = MakeDimWindow("VictoryWindow", Vec2F(0.0f, 22.0f), Vec2F(349.0f, 349.0f));
	victoryWindow->AddLayer("panel", mmake<Sprite>(String("victory.png")), Layout::BothStretch());

	// A random joke on the plate, between the crown and the button; the flow fills it on each win
	auto jokeText = mmake<Text>(String("debugFont.ttf"));
	jokeText->SetHeight(20);
	jokeText->SetColor(Color4(250, 226, 160));
	jokeText->SetHorAlign(HorAlign::Middle);
	jokeText->SetVerAlign(VerAlign::Middle);
	jokeText->SetWordWrap(true);
	victoryWindow->AddLayer("joke", jokeText,
							Layout(Vec2F(0.5f, 0.5f), Vec2F(0.5f, 0.5f), Vec2F(-153.0f, -52.0f), Vec2F(153.0f, 50.0f)));

	auto nextButton = MakeSpriteButton("VictoryWindowNextButton", "next_level.png",
									   Vec2F(0.0f, -103.0f), resultButtonSize);
	nextButton->onClick = [weakFlow] { if (auto f = weakFlow.Lock()) f->OnNextLevel(); };
	victoryWindow->AddChildWidget(nextButton);

	victoryWindow->SetParent(root);
	victoryWindow->SetEnabled(false);
	flow->victoryWindow.Set(victoryWindow.Get());

	// Game over: no panel — the red cross badge above two wide buttons on the darkened field
	auto gameOverWindow = MakeDimWindow("GameOverWindow", Vec2F(0.0f, -30.0f), Vec2F(349.0f, 250.0f));
	// The cross art is near-square (660x676); sized to keep its aspect, raised above the retry button
	gameOverWindow->AddLayer("cross", mmake<Sprite>(String("game_over.png")),
							 Layout(Vec2F(0.5f, 0.5f), Vec2F(0.5f, 0.5f), Vec2F(-62.0f, 100.0f), Vec2F(62.0f, 227.0f)));

	auto retryButton = MakeSpriteButton("GameOverWindowRetryButton", "retry.png",
										Vec2F(0.0f, 51.0f), resultButtonSize);
	retryButton->onClick = [weakFlow] { if (auto f = weakFlow.Lock()) f->OnRetry(); };
	gameOverWindow->AddChildWidget(retryButton);

	auto watchAdButton = MakeSpriteButton("GameOverWindowWatchAdButton", "watch_ad.png",
										  Vec2F(0.0f, -51.0f), resultButtonSize);
	watchAdButton->onClick = [weakFlow] { if (auto f = weakFlow.Lock()) f->OnContinueSameLevel(); };
	gameOverWindow->AddChildWidget(watchAdButton);

	gameOverWindow->SetParent(root);
	gameOverWindow->SetEnabled(false);
	flow->gameOverWindow.Set(gameOverWindow.Get());

	// Click sound for every result-window button, appended after the click handlers so the
	// windows' own onClick assignments don't overwrite it
	WeakRef<SoundComponent> weakClick(buttonClickSound.Get());
	for (auto& button : { nextButton, retryButton, watchAdButton })
	{
		button->onClick += [weakClick] {
			if (auto sound = weakClick.Lock())
				sound->RewindAndPlay();
		};
	}

	o2Scene.UpdateAddedEntities();
	o2Scene.UpdateTransforms();

	return root;
}
