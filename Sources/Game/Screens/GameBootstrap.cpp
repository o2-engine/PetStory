#include "o2/stdafx.h"
#include "Screens/GameBootstrap.h"

#include "Data/UserDataModel.h"
#include "Level/LevelChain.h"
#include "Screens/GameplayScreen.h"
#include "Screens/MetaScreen.h"
#include "GameLib/Localization/Localization.h"
#include "Windows/BuyMovesWindow.h"
#include "Windows/SettingsWindow.h"
#include "Windows/WinWindow.h"

const Ref<ScreenManager>& GameBootstrapComponent::GetScreens() const
{
	return mScreens;
}

const Ref<WindowManager>& GameBootstrapComponent::GetWindows() const
{
	return mWindows;
}

void GameBootstrapComponent::OnStart()
{
	Localization::LoadLanguage(mLanguagePath);
	LevelChain::Load(mChainPath);

	mWindows = mmake<WindowManager>();
	mWindows->AddWindow(mmake<SettingsWindow>());
	mWindows->AddWindow(mmake<WinWindow>());
	mWindows->AddWindow(mmake<BuyMovesWindow>());

	mScreens = mmake<ScreenManager>();
	mScreens->AddScreen(mmake<MetaScreen>());
	mScreens->AddScreen(mmake<GameplayScreen>());
	mScreens->ShowScreen(mStartScreen);
}

void GameBootstrapComponent::OnUpdate(float dt)
{
	if (mScreens)
		mScreens->Update(dt);
}

void GameBootstrapComponent::OnRemoveFromScene()
{
	if (mScreens)
	{
		mScreens->Clear();
		mScreens = nullptr;
	}

	if (mWindows)
	{
		mWindows->Clear();
		mWindows = nullptr;
	}
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<GameBootstrapComponent>);
// --- META ---

DECLARE_CLASS(GameBootstrapComponent, GameBootstrapComponent);
// --- END META ---
