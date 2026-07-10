#include "o2/stdafx.h"
#include "SlingPuckScene.h"

#include "o2/Animation/AnimationClip.h"
#include "o2/Render/Sprite.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/Components/ImageComponent.h"
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

// Result dialog assembled from engine UI widgets: a panel-art root widget (title and subtitle are
// baked into the panel image) with two sprite buttons at its bottom, hidden until a round ends
static Ref<Widget> MakeResultWindow(const String& name, const String& panelImage, const Vec2F& center,
									const String& leftButton, const Function<void()>& onLeftClick,
									const String& rightButton, const Function<void()>& onRightClick)
{
	const Vec2F panelSize(430.0f, 323.0f);  // panel art is 500x376, scaled into the field width
	const Vec2F buttonSize(150.0f, 104.0f);
	const Vec2F buttonOffset(92.0f, -78.0f); // button centres relative to the panel centre

	auto window = mmake<Widget>(ActorCreateMode::InScene);
	window->SetName(name);
	window->AddLayer("panel", mmake<Sprite>(panelImage), Layout::BothStretch());
	window->layout->anchorMin = Vec2F(0.5f, 0.5f);
	window->layout->anchorMax = Vec2F(0.5f, 0.5f);
	window->layout->offsetMin = center - panelSize * 0.5f;
	window->layout->offsetMax = center + panelSize * 0.5f;

	auto left = MakeSpriteButton(name + "LeftButton", leftButton, Vec2F(-buttonOffset.x, buttonOffset.y), buttonSize);
	left->onClick = onLeftClick;
	window->AddChildWidget(left);

	auto right = MakeSpriteButton(name + "RightButton", rightButton, Vec2F(buttonOffset.x, buttonOffset.y), buttonSize);
	right->onClick = onRightClick;
	window->AddChildWidget(right);

	return window;
}

Ref<Actor> BuildSlingPuckScene()
{
	// field.png (banner + frame + center separator are baked in). The separator sits below the
	// image centre, so the whole field is shifted up by fieldOffsetY to put it at world y = 0.
	const float fieldW = 500.0f;
	const float fieldH = 896.0f;
	const float fieldOffsetY = 47.0f;

	// Physics play area (centred on the divider at y = 0), aligned to the inner wood of field.png
	const float playHalfW = 222.0f;
	const float playHalfH = 378.0f;
	const float gapHalf = 42.0f;
	const float radius = 36.0f * 0.8f;

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
	board->halfHeight = playHalfH;
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

	// Rubber bands low/high near the back edges, spanning the full width to the side walls
	const float bandY = playHalfH - 56.0f;
	const float bandSpan = playHalfW + 18.0f;
	const float bandThickness = 16.0f;

	auto makeRubber = [&](int side, const Color4& bandColor) {
		float restY = side == 0 ? -bandY : bandY;

		auto rubberActor = mmake<Actor>(ActorCreateMode::InScene);
		rubberActor->SetName(side == 0 ? "RubberPlayer" : "RubberBot");
		rubberActor->transform->SetPosition2D(Vec2F(0.0f, 0.0f));
		auto rubber = rubberActor->AddComponent<SlingRubber>();
		rubber->side = side;
		rubber->restY = restY;
		rubber->halfSpan = bandSpan;
		rubber->thickness = bandThickness;
		rubber->color = bandColor;
		rubberActor->SetParent(root);
	};

	makeRubber(0, Color4(60, 120, 235, 255));  // player band, blue
	makeRubber(1, Color4(230, 60, 60, 255));   // bot band, red

	// Chips scattered across both halves (0 = blue, 1 = red, 2 = green)
	struct ChipSpec { int color; Vec2F pos; };
	const ChipSpec chips[] = {
		{ 1, Vec2F(160.0f, 120.0f) },  { 2, Vec2F(-150.0f, 130.0f) },
		{ 0, Vec2F(60.0f, 170.0f) },   { 1, Vec2F(-80.0f, 200.0f) },
		{ 2, Vec2F(170.0f, 250.0f) },  { 0, Vec2F(-20.0f, 280.0f) },
		{ 1, Vec2F(-120.0f, 300.0f) }, { 0, Vec2F(70.0f, 330.0f) },
		{ 0, Vec2F(-160.0f, -120.0f) }, { 1, Vec2F(150.0f, -130.0f) },
		{ 2, Vec2F(-60.0f, -170.0f) },  { 0, Vec2F(80.0f, -200.0f) },
		{ 1, Vec2F(-170.0f, -250.0f) }, { 2, Vec2F(20.0f, -280.0f) },
		{ 0, Vec2F(120.0f, -300.0f) },  { 1, Vec2F(-70.0f, -330.0f) },
	};

	for (auto& spec : chips)
	{
		String image = spec.color == 0 ? "blue_chip.png" : (spec.color == 1 ? "red_chip.png" : "green_chip.png");
		auto actor = MakeSprite("Chip", spec.pos, Vec2F(radius * 2.0f, radius * 2.0f), image);

		auto puck = actor->AddComponent<SlingPuck>();
		puck->team = spec.color;
		puck->radius = radius;
		puck->dragPower = 40.0f; // shallow pulls (the band stays inside the field) still shoot hard
		puck->position = spec.pos;

		actor->SetParent(root);
	}

	// Divider cap, added last so it draws above the chips and they slide under it through the gap.
	// Sized and placed so the solid bar of separator.png (rows 219-322, cols 12-2275 of 2276x464)
	// lands exactly on the divider baked into field.png (world y in [-1.5, 17.7], x in ±239.6)
	MakeSprite("Separator", Vec2F(-1.2f, 15.2f), Vec2F(481.7f, 85.7f), "separator.png")->SetParent(root);

	// Result dialogs, after everything else so they draw on top; the flow shows one per round end
	WeakRef<SlingGameFlow> weakFlow(flow.Get());
	Vec2F windowCenter(0.0f, fieldOffsetY);

	auto victoryWindow = MakeResultWindow("VictoryWindow", "victory.png", windowCenter,
		"next_level.png", [weakFlow] { if (auto f = weakFlow.Lock()) f->OnNextLevel(); },
		"leaderboard.png", [] { o2Debug.Log("Leaderboard is not implemented yet"); });
	victoryWindow->SetParent(root);
	victoryWindow->SetEnabled(false);
	flow->victoryWindow.Set(victoryWindow.Get());

	auto gameOverWindow = MakeResultWindow("GameOverWindow", "game_over.png", windowCenter,
		"retry.png", [weakFlow] { if (auto f = weakFlow.Lock()) f->OnRetry(); },
		"watch_ad.png", [weakFlow] { if (auto f = weakFlow.Lock()) f->OnContinueSameLevel(); });
	gameOverWindow->SetParent(root);
	gameOverWindow->SetEnabled(false);
	flow->gameOverWindow.Set(gameOverWindow.Get());

	o2Scene.UpdateAddedEntities();
	o2Scene.UpdateTransforms();

	return root;
}
