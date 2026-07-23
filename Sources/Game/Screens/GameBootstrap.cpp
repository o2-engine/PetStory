#include "o2/stdafx.h"
#include "Screens/GameBootstrap.h"

#include "Progress/GameProgress.h"
#include "Screens/GameplayScreen.h"
#include "Screens/MetaScreen.h"

const Ref<ScreenManager>& GameBootstrapComponent::GetScreens() const
{
	return mScreens;
}

void GameBootstrapComponent::OnStart()
{
	GameProgress::LoadChain(mChainPath);

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
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<GameBootstrapComponent>);
// --- META ---

DECLARE_CLASS(GameBootstrapComponent, GameBootstrapComponent);
// --- END META ---
