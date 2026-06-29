#include "o2/stdafx.h"
#include "SlingPuckScene.h"

#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Utils/Math/Color.h"

#include "SlingBoard.h"
#include "SlingBot.h"
#include "SlingGameController.h"
#include "SlingPuck.h"
#include "SlingRubber.h"

static Ref<Actor> MakeSprite(const String& name, const Vec2F& pos, const Vec2F& size, const String& imagePath)
{
	auto actor = mmake<Actor>(ActorCreateMode::InScene);
	actor->SetName(name);
	actor->transform->SetPivot(Vec2F(0.5f, 0.5f));
	actor->transform->SetSize(size);
	actor->transform->SetPosition(pos);
	actor->AddComponent(mmake<ImageComponent>(imagePath));
	return actor;
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
	const float radius = 36.0f;

	auto camera = mmake<CameraActor>();
	camera->SetName("Camera");
	camera->fillColor = Color4(24, 26, 34);
	camera->SetFittedSize(Vec2F(fieldW + 40.0f, fieldH + 100.0f));
	camera->AddToScene();
	camera->transform->SetPosition(Vec2F(0.0f, fieldOffsetY));

	auto root = mmake<Actor>(ActorCreateMode::InScene);
	root->SetName("SlingGame");
	root->transform->SetPosition(Vec2F(0.0f, 0.0f));

	auto board = root->AddComponent<SlingBoard>();
	auto bot = root->AddComponent<SlingBot>();
	auto controller = root->AddComponent<SlingGameController>();

	board->halfWidth = playHalfW;
	board->halfHeight = playHalfH;
	board->gapHalf = gapHalf;
	board->friction = 0.7f; // less drag -> chips glide faster and further

	bot->board.Set(board.Get());
	controller->board.Set(board.Get());
	controller->bot.Set(bot.Get());

	MakeSprite("Field", Vec2F(0.0f, fieldOffsetY), Vec2F(fieldW, fieldH), "field.png")->SetParent(root);

	// Rubber bands low/high near the back edges, spanning the full width to the side walls
	const float bandY = playHalfH - 56.0f;
	const float bandSpan = playHalfW + 18.0f;
	const float bandThickness = 16.0f;

	auto makeRubber = [&](int side, const String& image) {
		float restY = side == 0 ? -bandY : bandY;

		auto rubberActor = mmake<Actor>(ActorCreateMode::InScene);
		rubberActor->SetName(side == 0 ? "RubberPlayer" : "RubberBot");
		rubberActor->transform->SetPosition(Vec2F(0.0f, 0.0f));
		auto rubber = rubberActor->AddComponent<SlingRubber>();
		rubber->side = side;
		rubber->restY = restY;
		rubber->halfSpan = bandSpan;
		rubber->thickness = bandThickness;
		rubberActor->SetParent(root);

		// Two leg sprites as children of the rubber actor (gathered by SlingRubber::OnStart)
		MakeSprite("LegLeft", Vec2F(0.0f, restY), Vec2F(bandSpan * 2.0f, bandThickness), image)->SetParent(rubberActor);
		MakeSprite("LegRight", Vec2F(0.0f, restY), Vec2F(0.0f, 0.0f), image)->SetParent(rubberActor);
	};

	makeRubber(0, "rubber_blue.png");
	makeRubber(1, "rubber_red.png");

	// Chips scattered across both halves (0 = blue, 1 = red, 2 = green)
	struct ChipSpec { int color; Vec2F pos; };
	const ChipSpec chips[] = {
		{ 1, Vec2F(-156.0f, 241.0f) }, { 2, Vec2F(69.0f, 241.0f) },
		{ 0, Vec2F(-48.0f, 156.0f) },  { 1, Vec2F(154.0f, 101.0f) },
		{ 0, Vec2F(-152.0f, -91.0f) }, { 2, Vec2F(46.0f, -136.0f) },
		{ 1, Vec2F(-78.0f, -204.0f) }, { 0, Vec2F(147.0f, -207.0f) },
	};

	for (auto& spec : chips)
	{
		String image = spec.color == 0 ? "blue_chip.png" : (spec.color == 1 ? "red_chip.png" : "green_chip.png");
		auto actor = MakeSprite("Chip", spec.pos, Vec2F(radius * 2.0f, radius * 2.0f), image);

		auto puck = actor->AddComponent<SlingPuck>();
		puck->team = spec.color;
		puck->radius = radius;
		puck->position = spec.pos;

		actor->SetParent(root);
	}

	o2Scene.UpdateAddedEntities();
	o2Scene.UpdateTransforms();

	return root;
}
