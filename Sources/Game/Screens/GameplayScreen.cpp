#include "o2/stdafx.h"
#include "Screens/GameplayScreen.h"

#include "Level/ChipColors.h"
#include "Level/LevelBuilder.h"
#include "Level/LevelController.h"
#include "Progress/GameProgress.h"
#include "Screens/MetaScreen.h"
#include "Screens/ScreenManager.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/WidgetLayout.h"
#include "o2/Utils/Debug/Debug.h"

namespace
{
	const Vec2F kDesignSize(2160.0f, 3840.0f);
	const char* kGoalsFont = "Fonts/GrilledCheese BTN.ttf";
	const float kCompleteSwitchDelay = 1.0f;

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

	Ref<Label> MakeLabel(const String& name, const WString& text, const Vec2F& pos, const Vec2F& size, int height)
	{
		auto label = mmake<Label>();
		label->SetName(name);
		label->SetFontAsset(AssetRef<FontAsset>(kGoalsFont));
		label->SetText(text);
		label->SetHeight(height);
		label->SetHorAlign(HorAlign::Middle);
		label->SetVerAlign(VerAlign::Middle);
		label->layout->anchorMin = Vec2F(0.5f, 0.5f);
		label->layout->anchorMax = Vec2F(0.5f, 0.5f);
		label->layout->offsetMin = pos - size * 0.5f;
		label->layout->offsetMax = pos + size * 0.5f;
		return label;
	}
}

Ref<LevelController> GameplayScreen::GetLevelController() const
{
	return mController.Lock();
}

LevelData GameplayScreen::LoadLevelData(const String& assetPath)
{
	LevelData data;
	if (!assetPath.IsEmpty() && data.LoadFromAsset(assetPath))
		return data;

	o2Debug.LogWarning("GameplayScreen: level asset '" + assetPath + "' is missing, using fallback level");

	data = LevelData();
	data.name = "Fallback";
	data.border = { Vec2F(-900.0f, -1500.0f), Vec2F(900.0f, -1500.0f),
					Vec2F(900.0f, 1100.0f), Vec2F(-900.0f, 1100.0f) };

	LevelSpawnPoint spawner;
	spawner.position = Vec2F(0.0f, 900.0f);
	spawner.zoneSize = Vec2F(1500.0f, 150.0f);
	spawner.colors = { "Blue", "Green", "Red" };
	spawner.maxOnScreen = 20;
	data.spawners.Add(spawner);

	LevelGoal goal;
	goal.chipType = "Green";
	goal.count = 10;
	data.goals.Add(goal);

	return data;
}

void GameplayScreen::OnLoad()
{
	mRoot = mmake<Actor>(ActorCreateMode::InScene);
	mRoot->SetName("GameplayScreen");
	mRoot->transform->SetPosition2D(Vec2F());

	auto camera = mmake<CameraActor>();
	camera->SetName("Camera");
	camera->fillColor = Color4(64, 46, 76);
	camera->SetFittedSize(kDesignSize);
	camera->SetParent(mRoot);
	camera->transform->SetPosition2D(Vec2F());

	MakeSprite("Back", Vec2F(), kDesignSize, "Game field/Back.png")->SetParent(mRoot);

	LevelData data = LoadLevelData(GameProgress::GetCurrentLevelPath());

	mLevelRoot = BuildLevel(data);
	mLevelRoot->SetParent(mRoot);

	auto controller = mLevelRoot->GetComponent<LevelController>();
	mController = controller;

	WeakRef<GameplayScreen> weakThis(this);
	controller->onCompleted = [weakThis] {
		if (auto screen = weakThis.Lock())
			screen->OnLevelCompleted();
	};
	controller->onGoalsChanged = [weakThis] {
		if (auto screen = weakThis.Lock())
			screen->UpdateGoalLabels();
	};

	BuildGoalsBubble(data);

	mCompleteTimer = -1.0f;

	o2Scene.UpdateAddedEntities();
	o2Scene.UpdateTransforms();
}

void GameplayScreen::BuildGoalsBubble(const LevelData& data)
{
	const Vec2F bubbleSize(809.0f, 420.0f);
	const Vec2F bubblePos(kDesignSize.x * 0.5f - bubbleSize.x * 0.5f - 60.0f,
						  kDesignSize.y * 0.5f - bubbleSize.y * 0.5f - 120.0f);

	auto bubble = MakeSprite("GoalsBubble", bubblePos, bubbleSize, "Game field/UI/GoalsBack.png");
	bubble->SetParent(mRoot);

	mGoalLabels.Clear();

	int goalsCount = data.goals.Count();
	if (goalsCount == 0)
		return;

	// Goals in a row inside the bubble: chip icon with the counter label below
	const float iconSize = 170.0f;
	const float step = Math::Min(200.0f, (bubbleSize.x - 100.0f) / (float)goalsCount);
	const float firstX = -step * 0.5f * (float)(goalsCount - 1);

	for (int i = 0; i < goalsCount; i++)
	{
		Vec2F iconPos = bubblePos + Vec2F(firstX + step * (float)i, 45.0f);

		String iconPath = ChipColors::GetIconPath(data.goals[i].chipType);
		if (!iconPath.IsEmpty())
			MakeSprite(String("GoalIcon") + (String)i, iconPos, Vec2F(iconSize, iconSize), iconPath)->SetParent(mRoot);

		auto label = MakeLabel(String("GoalLabel") + (String)i, "0", iconPos + Vec2F(0.0f, -140.0f),
							   Vec2F(220.0f, 110.0f), 72);
		label->SetColor(Color4(92, 62, 41));
		label->SetParent(mRoot);
		mGoalLabels.Add(label);
	}

	UpdateGoalLabels();
}

void GameplayScreen::UpdateGoalLabels()
{
	auto controller = mController.Lock();
	if (!controller)
		return;

	auto& goals = controller->GetGoals();
	for (int i = 0; i < mGoalLabels.Count() && i < goals.Count(); i++)
	{
		int left = Math::Max(0, goals[i].count - controller->GetCollected(i));
		mGoalLabels[i]->SetText((WString)(String)left);
	}
}

void GameplayScreen::OnLevelCompleted()
{
	if (mCompleteTimer < 0.0f)
		mCompleteTimer = kCompleteSwitchDelay;
}

void GameplayScreen::OnUpdate(float dt)
{
	if (mCompleteTimer >= 0.0f)
	{
		mCompleteTimer -= dt;
		if (mCompleteTimer < 0.0f)
		{
			GameProgress::AdvanceLevel();

			if (auto manager = ScreenManager::Instance())
				manager->ShowScreen(MetaScreen::kName);
		}
	}
}

void GameplayScreen::OnUnload()
{
	mGoalLabels.Clear();
	mController = nullptr;
	mLevelRoot = nullptr;

	if (mRoot)
	{
		mRoot->SetEnabled(false);
		o2Scene.DestroyActor(mRoot);
		mRoot = nullptr;
	}
}

void GameplayScreen::OnActivated()
{
	if (mRoot)
		mRoot->SetEnabled(true);
}

void GameplayScreen::OnDeactivated()
{
	if (mRoot)
		mRoot->SetEnabled(false);
}
