#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "Progress/GameProgress.h"

using namespace o2;

namespace
{
	struct ProgressGuard
	{
		~ProgressGuard() { GameProgress::Reset(); }
	};
}

TEST(GameProgressTests, AdvanceWalksChainAndWraps)
{
	ProgressGuard guard;
	GameProgress::Reset();
	GameProgress::SetChain({ "a.json", "b.json", "c.json" });

	EXPECT_EQ(GameProgress::GetLevelsCount(), 3);
	EXPECT_EQ(GameProgress::GetCurrentLevel(), 0);
	EXPECT_EQ(GameProgress::GetCurrentLevelPath(), "a.json");

	GameProgress::AdvanceLevel();
	EXPECT_EQ(GameProgress::GetCurrentLevelPath(), "b.json");

	GameProgress::AdvanceLevel();
	GameProgress::AdvanceLevel();
	EXPECT_EQ(GameProgress::GetCurrentLevel(), 0);
	EXPECT_EQ(GameProgress::GetCurrentLevelPath(), "a.json");
}

TEST(GameProgressTests, SetCurrentLevelClampsToChain)
{
	ProgressGuard guard;
	GameProgress::Reset();
	GameProgress::SetChain({ "a.json", "b.json" });

	GameProgress::SetCurrentLevel(5);
	EXPECT_EQ(GameProgress::GetCurrentLevel(), 1);

	GameProgress::SetCurrentLevel(-2);
	EXPECT_EQ(GameProgress::GetCurrentLevel(), 0);
}

TEST(GameProgressTests, EmptyChainIsSafe)
{
	ProgressGuard guard;
	GameProgress::Reset();

	EXPECT_EQ(GameProgress::GetLevelsCount(), 0);
	EXPECT_EQ(GameProgress::GetCurrentLevelPath(), "");
	GameProgress::AdvanceLevel();
	EXPECT_EQ(GameProgress::GetCurrentLevel(), 0);
}

TEST(GameProgressTests, LoadsShippedChain)
{
	ProgressGuard guard;
	GameProgress::Reset();

	ASSERT_TRUE(GameProgress::LoadChain());
	EXPECT_EQ(GameProgress::GetLevelsCount(), 10);
	EXPECT_EQ(GameProgress::GetCurrentLevelPath(), "Levels/Level01.json");
}

TEST(GameProgressTests, MissingChainAssetFails)
{
	ProgressGuard guard;
	GameProgress::Reset();

	EXPECT_FALSE(GameProgress::LoadChain("Levels/NoSuchChain.json"));
	EXPECT_EQ(GameProgress::GetLevelsCount(), 0);
}
