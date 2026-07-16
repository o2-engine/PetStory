#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "Jokes.h"
#include "Localization.h"
#include "YandexGames.h"

using namespace o2;

namespace
{
	// Tests of one suite share a process (batch ctest): always restore the global language
	struct LangGuard
	{
		Loc::Lang saved = Loc::GetLanguage();
		~LangGuard() { Loc::SetLanguage(saved); }
	};
}

TEST(Localization, TrPicksTheCurrentLanguageVariant)
{
	LangGuard guard;

	Loc::SetLanguage(Loc::Lang::Russian);
	EXPECT_EQ(Loc::Tr("да", "yes"), String("да"));

	Loc::SetLanguage(Loc::Lang::English);
	EXPECT_EQ(Loc::Tr("да", "yes"), String("yes"));
}

TEST(Localization, LanguageCodeMapsRuToRussianAndAnythingElseToEnglish)
{
	LangGuard guard;

	Loc::SetLanguage(Loc::Lang::English);
	Loc::SetLanguageFromCode("ru");
	EXPECT_EQ(Loc::GetLanguage(), Loc::Lang::Russian);

	Loc::SetLanguageFromCode("en");
	EXPECT_EQ(Loc::GetLanguage(), Loc::Lang::English);

	Loc::SetLanguageFromCode("tr");
	EXPECT_EQ(Loc::GetLanguage(), Loc::Lang::English);

	// no SDK -> empty code -> the current language stays
	Loc::SetLanguage(Loc::Lang::Russian);
	Loc::SetLanguageFromCode("");
	EXPECT_EQ(Loc::GetLanguage(), Loc::Lang::Russian);
}

TEST(Localization, EveryJokeHasBothLanguagesAndTheyDiffer)
{
	LangGuard guard;

	for (int i = 0; i < Jokes::Count(); i++)
	{
		Loc::SetLanguage(Loc::Lang::Russian);
		String russian = Jokes::At(i);

		Loc::SetLanguage(Loc::Lang::English);
		String english = Jokes::At(i);

		EXPECT_FALSE(russian.IsEmpty()) << "joke " << i;
		EXPECT_FALSE(english.IsEmpty()) << "joke " << i;
		EXPECT_NE(russian, english) << "joke " << i;
	}
}

// Off-wasm the SDK stub reports an instantly granted reward, so the WATCH AD flow stays
// playable in desktop runs and tests; the result is consumed exactly once
TEST(YandexGamesStub, RewardedResultIsGrantedOnceAndCleared)
{
	EXPECT_EQ(YandexGames::PopRewardedResult(), -1);

	YandexGames::ShowRewardedVideo();
	EXPECT_EQ(YandexGames::PopRewardedResult(), 1);
	EXPECT_EQ(YandexGames::PopRewardedResult(), -1);

	EXPECT_TRUE(YandexGames::GetLanguage().IsEmpty());
}
