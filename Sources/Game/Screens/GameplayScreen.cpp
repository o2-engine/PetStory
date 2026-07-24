#include "o2/stdafx.h"
#include "Screens/GameplayScreen.h"

#include "Level/ChipColors.h"
#include "Level/LevelBuilder.h"
#include "Level/LevelController.h"
#include "Data/UserDataModel.h"
#include "Level/LevelChain.h"
#include "Screens/MetaScreen.h"
#include "GameLib/Localization/Localization.h"
#include "GameLib/Screens/ScreenManager.h"
#include "GameLib/Windows/WindowManager.h"
#include "Windows/BuyMovesWindow.h"
#include "Windows/WinWindow.h"
#include "o2/Render/Render.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Scene/Scene.h"
#include "o2/Scene/UI/WidgetLayout.h"
#include "o2/Utils/Debug/Debug.h"

namespace
{
	const Vec2F kDesignSize(2160.0f, 3840.0f);
	const char* kGoalsFont = "Fonts/GrilledCheese BTN.ttf";
	const float kCompleteWindowDelay = 1.0f;

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

	Ref<Label> MakeLabel(const String& name, const WString& text, const Vec2F& pos, const Vec2F& size, int height)
	{
		// Even the Label constructor loads a font, so no labels at all without render
		if (!Render::IsSingletonInitialzed())
			return nullptr;

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

String GameplayScreen::GetName() const
{
	return kName;
}

const Ref<Actor>& GameplayScreen::GetRoot() const
{
	return mRoot;
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

	LevelData data = LoadLevelData(LevelChain::LevelPath(UserDataModel::Get().currentLevel));

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
	controller->onMovesChanged = [weakThis] {
		if (auto screen = weakThis.Lock())
			screen->UpdateMovesLabel();
	};
	controller->onOutOfMoves = [weakThis] {
		if (auto screen = weakThis.Lock())
			screen->OnOutOfMoves();
	};

	BuildGoalsBubble(data);

	if (controller->HasMovesLimit())
	{
		mMovesLabel = MakeLabel("MovesLabel", "0", Vec2F(-950.0f, 1810.0f), Vec2F(400.0f, 160.0f), 110);
		if (mMovesLabel)
		{
			mMovesLabel->SetParent(mRoot);
			UpdateMovesLabel();
		}
	}

	mCompleteTimer = -1.0f;
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
		if (label)
		{
			label->SetColor(Color4(92, 62, 41));
			label->SetParent(mRoot);
			mGoalLabels.Add(label);
		}
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

void GameplayScreen::UpdateMovesLabel()
{
	auto controller = mController.Lock();
	if (!controller || !mMovesLabel)
		return;

	mMovesLabel->SetText((WString)(String)controller->GetMovesLeft());
}

void GameplayScreen::OnLevelCompleted()
{
	if (mCompleteTimer < 0.0f)
		mCompleteTimer = kCompleteWindowDelay;
}

int GameplayScreen::ComputeStars() const
{
	auto controller = mController.Lock();
	if (!controller || !controller->HasMovesLimit())
		return 3;

	float leftShare = (float)controller->GetMovesLeft() / (float)controller->GetMovesLimit();
	if (leftShare >= 0.4f)
		return 3;
	if (leftShare >= 0.15f)
		return 2;

	return 1;
}

void GameplayScreen::ShowWinWindow()
{
	auto windows = WindowManager::Instance();
	auto window = windows ? windows->GetWindow(WinWindow::kName) : nullptr;

	// Without the window system the win still advances the game
	if (!window)
	{
		UserDataModel::AdvanceLevel(LevelChain::Count());

		if (auto manager = ScreenManager::Instance())
			manager->ShowScreen(MetaScreen::kName);

		return;
	}

	window->Load();
	window->SetScriptProperty("stars", ComputeStars());

	window->onAction = [](const String& action) {
		if (action != "next")
			return;

		if (auto windows = WindowManager::Instance())
			windows->HideWindow(WinWindow::kName);

		UserDataModel::AdvanceLevel(LevelChain::Count());

		if (auto manager = ScreenManager::Instance())
			manager->ShowScreen(MetaScreen::kName);
	};

	window->Show();
}

void GameplayScreen::OnOutOfMoves()
{
	auto windows = WindowManager::Instance();
	auto window = windows ? windows->GetWindow(BuyMovesWindow::kName) : nullptr;
	if (!window)
		return;

	window->Load();
	window->SetScriptProperty("coins", UserDataModel::Get().coins);

	// The offer text carries the actual price: formatted here, not a static key
	if (window->GetRoot())
	{
		if (auto offerLabel = DynamicCast<Label>(window->GetRoot()->FindChild("OfferLabel")))
		{
			offerLabel->SetText(Localization::Format("buyMoves.offer", {
				{ "moves", (WString)(String)BuyMovesWindow::kMoves },
				{ "price", (WString)(String)BuyMovesWindow::kPrice } }));
		}
	}

	WeakRef<GameplayScreen> weakThis(this);
	window->onAction = [weakThis](const String& action) {
		auto windows = WindowManager::Instance();

		if (action == "buy")
		{
			auto screen = weakThis.Lock();
			auto controller = screen ? screen->GetLevelController() : nullptr;
			if (!controller)
				return;

			if (UserDataModel::TrySpendCoins(BuyMovesWindow::kPrice))
			{
				controller->AddMoves(BuyMovesWindow::kMoves);

				if (windows)
					windows->HideWindow(BuyMovesWindow::kName);
			}
		}
		else if (action == "close")
		{
			if (windows)
				windows->HideWindow(BuyMovesWindow::kName);

			if (auto manager = ScreenManager::Instance())
				manager->ShowScreen(MetaScreen::kName);
		}
	};

	window->Show();
}

void GameplayScreen::OnUpdate(float dt)
{
	if (mCompleteTimer >= 0.0f)
	{
		mCompleteTimer -= dt;
		if (mCompleteTimer < 0.0f)
			ShowWinWindow();
	}
}

void GameplayScreen::OnUnload()
{
	if (auto windows = WindowManager::Instance())
		windows->UnloadAll();

	mGoalLabels.Clear();
	mMovesLabel = nullptr;
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
