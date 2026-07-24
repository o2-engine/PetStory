#include "o2/stdafx.h"
#include "GameLib/Screens/GameScreen.h"

GameScreen::~GameScreen() = default;

void GameScreen::Load()
{
	if (mLoaded)
		return;

	mLoaded = true;
	OnLoad();
}

void GameScreen::Unload()
{
	if (!mLoaded)
		return;

	if (mActive)
		Deactivate();

	mLoaded = false;
	OnUnload();
}

void GameScreen::Activate()
{
	if (mActive)
		return;

	if (!mLoaded)
		Load();

	mActive = true;
	OnActivated();
}

void GameScreen::Deactivate()
{
	if (!mActive)
		return;

	mActive = false;
	OnDeactivated();
}

void GameScreen::Update(float dt)
{
	if (mActive)
		OnUpdate(dt);
}

bool GameScreen::IsLoaded() const
{
	return mLoaded;
}

bool GameScreen::IsActive() const
{
	return mActive;
}

void GameScreen::OnLoad()
{}

void GameScreen::OnUnload()
{}

void GameScreen::OnActivated()
{}

void GameScreen::OnDeactivated()
{}

void GameScreen::OnUpdate(float dt)
{}
