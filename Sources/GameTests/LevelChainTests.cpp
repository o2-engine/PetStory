#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "Level/LevelChain.h"

using namespace o2;

namespace
{
	struct ChainGuard
	{
		~ChainGuard() { LevelChain::Reset(); }
	};
}

TEST(LevelChainTests, SetAndLookup)
{
	ChainGuard guard;
	LevelChain::Set({ "a.json", "b.json", "c.json" });

	EXPECT_EQ(LevelChain::Count(), 3);
	EXPECT_EQ(LevelChain::LevelPath(0), "a.json");
	EXPECT_EQ(LevelChain::LevelPath(2), "c.json");
	EXPECT_EQ(LevelChain::LevelPath(3), "");
	EXPECT_EQ(LevelChain::LevelPath(-1), "");
}

TEST(LevelChainTests, EmptyChainIsSafe)
{
	ChainGuard guard;
	LevelChain::Reset();

	EXPECT_EQ(LevelChain::Count(), 0);
	EXPECT_EQ(LevelChain::LevelPath(0), "");
}

TEST(LevelChainTests, LoadsShippedChain)
{
	ChainGuard guard;

	ASSERT_TRUE(LevelChain::Load());
	EXPECT_EQ(LevelChain::Count(), 10);
	EXPECT_EQ(LevelChain::LevelPath(0), "Levels/Level01.json");
}

TEST(LevelChainTests, MissingChainAssetFails)
{
	ChainGuard guard;
	LevelChain::Reset();

	EXPECT_FALSE(LevelChain::Load("Levels/NoSuchChain.json"));
	EXPECT_EQ(LevelChain::Count(), 0);
}
