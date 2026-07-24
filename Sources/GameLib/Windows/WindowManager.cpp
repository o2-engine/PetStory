#include "o2/stdafx.h"
#include "GameLib/Windows/WindowManager.h"

#include "o2/Utils/Debug/Debug.h"

WindowManager* WindowManager::sInstance = nullptr;

WindowManager::WindowManager()
{
	if (!sInstance)
		sInstance = this;
}

WindowManager::~WindowManager()
{
	if (sInstance == this)
		sInstance = nullptr;
}

WindowManager* WindowManager::Instance()
{
	return sInstance;
}

void WindowManager::AddWindow(const Ref<GameWindow>& window)
{
	if (GetWindow(window->GetName()))
	{
		o2Debug.LogError("WindowManager: window '" + window->GetName() + "' is already registered");
		return;
	}

	mWindows.Add(window);
}

Ref<GameWindow> WindowManager::GetWindow(const String& name) const
{
	for (auto& window : mWindows)
	{
		if (window->GetName() == name)
			return window;
	}

	return nullptr;
}

Ref<GameWindow> WindowManager::ShowWindow(const String& name)
{
	auto window = GetWindow(name);
	if (!window)
	{
		o2Debug.LogError("WindowManager: unknown window '" + name + "'");
		return nullptr;
	}

	window->Show();
	return window;
}

void WindowManager::HideWindow(const String& name)
{
	if (auto window = GetWindow(name))
		window->Hide();
}

void WindowManager::HideAll()
{
	for (auto& window : mWindows)
		window->Hide();
}

void WindowManager::UnloadAll()
{
	for (auto& window : mWindows)
		window->Unload();
}

void WindowManager::Clear()
{
	UnloadAll();
	mWindows.Clear();
}
