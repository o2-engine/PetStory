#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "Screens/ScreenManager.h"

using namespace o2;

namespace
{
	struct TestScreen: GameScreen
	{
		String name;
		Vector<String>* log = nullptr;

		TestScreen(const String& name, Vector<String>* log): name(name), log(log) {}

		String GetName() const override { return name; }

		void OnLoad() override { log->Add(name + ":load"); }
		void OnUnload() override { log->Add(name + ":unload"); }
		void OnActivated() override { log->Add(name + ":activate"); }
		void OnDeactivated() override { log->Add(name + ":deactivate"); }
		void OnUpdate(float dt) override { log->Add(name + ":update"); }
	};
}

TEST(ScreenManagerTests, ShowIsDeferredUntilUpdate)
{
	Vector<String> log;
	auto manager = mmake<ScreenManager>();
	manager->AddScreen(mmake<TestScreen>("A", &log));

	manager->ShowScreen("A");
	EXPECT_TRUE(log.IsEmpty());
	EXPECT_EQ(manager->GetCurrentScreen(), nullptr);

	manager->Update(0.1f);
	EXPECT_EQ(log, Vector<String>({ "A:load", "A:activate", "A:update" }));
	EXPECT_EQ(manager->GetCurrentScreen()->GetName(), "A");
}

TEST(ScreenManagerTests, SwitchUnloadsOldBeforeLoadingNew)
{
	Vector<String> log;
	auto manager = mmake<ScreenManager>();
	manager->AddScreen(mmake<TestScreen>("A", &log));
	manager->AddScreen(mmake<TestScreen>("B", &log));

	manager->ShowScreen("A");
	manager->Update(0.1f);
	log.Clear();

	manager->ShowScreen("B");
	manager->Update(0.1f);

	EXPECT_EQ(log, Vector<String>({ "A:deactivate", "A:unload", "B:load", "B:activate", "B:update" }));
	EXPECT_EQ(manager->GetCurrentScreen()->GetName(), "B");
	EXPECT_FALSE(manager->GetScreen("A")->IsLoaded());
}

TEST(ScreenManagerTests, ShowCurrentScreenAgainIsNoOp)
{
	Vector<String> log;
	auto manager = mmake<ScreenManager>();
	manager->AddScreen(mmake<TestScreen>("A", &log));

	manager->ShowScreen("A");
	manager->Update(0.1f);
	log.Clear();

	manager->ShowScreen("A");
	manager->Update(0.1f);
	EXPECT_EQ(log, Vector<String>({ "A:update" }));
}

TEST(ScreenManagerTests, UnknownScreenIsIgnored)
{
	Vector<String> log;
	auto manager = mmake<ScreenManager>();
	manager->AddScreen(mmake<TestScreen>("A", &log));

	manager->ShowScreen("Missing");
	manager->Update(0.1f);
	EXPECT_EQ(manager->GetCurrentScreen(), nullptr);
	EXPECT_TRUE(log.IsEmpty());
}

TEST(ScreenManagerTests, ReturningToScreenReloadsIt)
{
	Vector<String> log;
	auto manager = mmake<ScreenManager>();
	manager->AddScreen(mmake<TestScreen>("A", &log));
	manager->AddScreen(mmake<TestScreen>("B", &log));

	manager->ShowScreen("A");
	manager->Update(0.1f);
	manager->ShowScreen("B");
	manager->Update(0.1f);
	log.Clear();

	manager->ShowScreen("A");
	manager->Update(0.1f);
	EXPECT_EQ(log, Vector<String>({ "B:deactivate", "B:unload", "A:load", "A:activate", "A:update" }));
}

TEST(ScreenManagerTests, ClearUnloadsCurrent)
{
	Vector<String> log;
	auto manager = mmake<ScreenManager>();
	manager->AddScreen(mmake<TestScreen>("A", &log));

	manager->ShowScreen("A");
	manager->Update(0.1f);
	log.Clear();

	manager->Clear();
	EXPECT_EQ(log, Vector<String>({ "A:deactivate", "A:unload" }));
	EXPECT_EQ(manager->GetCurrentScreen(), nullptr);
	EXPECT_EQ(manager->GetScreen("A"), nullptr);
}

TEST(ScreenManagerTests, GlobalInstanceFollowsLifetime)
{
	{
		auto manager = mmake<ScreenManager>();
		EXPECT_EQ(ScreenManager::Instance(), manager.Get());
	}
	EXPECT_EQ(ScreenManager::Instance(), nullptr);
}
