#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "GameLib/Localization/Localization.h"

using namespace o2;

namespace
{
	struct LocalizationGuard
	{
		~LocalizationGuard() { Localization::Reset(); }
	};
}

TEST(LocalizationTests, LoadsShippedLanguages)
{
	LocalizationGuard guard;

	ASSERT_TRUE(Localization::LoadLanguage("Localization/ru.json"));
	EXPECT_EQ(Localization::GetLanguageAssetPath(), "Localization/ru.json");
	EXPECT_TRUE(Localization::HasText("win.title"));
	EXPECT_EQ(Localization::GetText("win.next"), WString(L"Дальше"));

	ASSERT_TRUE(Localization::LoadLanguage("Localization/en.json"));
	EXPECT_EQ(Localization::GetText("win.next"), WString("Next"));
}

TEST(LocalizationTests, MissingKeyFallsBackToKey)
{
	LocalizationGuard guard;

	ASSERT_TRUE(Localization::LoadLanguage("Localization/en.json"));
	EXPECT_FALSE(Localization::HasText("no.such.key"));
	EXPECT_EQ(Localization::GetText("no.such.key"), WString("no.such.key"));
}

TEST(LocalizationTests, FormatReplacesPlaceholders)
{
	LocalizationGuard guard;

	ASSERT_TRUE(Localization::LoadLanguage("Localization/en.json"));
	EXPECT_EQ(Localization::Format("buyMoves.offer", { { "moves", WString("5") }, { "price", WString("10") } }),
			  WString("Buy 5 moves for 10"));
}

TEST(LocalizationTests, MissingLanguageAssetFails)
{
	LocalizationGuard guard;

	EXPECT_FALSE(Localization::LoadLanguage("Localization/nope.json"));
	EXPECT_EQ(Localization::GetLanguageAssetPath(), "");
}

TEST(LocalizationTests, LanguageChangeNotifies)
{
	LocalizationGuard guard;

	int changes = 0;
	auto handler = [&] { changes++; };
	Localization::OnLanguageChanged() += handler;

	ASSERT_TRUE(Localization::LoadLanguage("Localization/ru.json"));
	EXPECT_EQ(changes, 1);

	ASSERT_TRUE(Localization::LoadLanguage("Localization/en.json"));
	EXPECT_EQ(changes, 2);

	Localization::LoadLanguage("Localization/nope.json");
	EXPECT_EQ(changes, 2);

	Localization::OnLanguageChanged() -= handler;
}