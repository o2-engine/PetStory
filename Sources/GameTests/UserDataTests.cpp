#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "Data/UserDataModel.h"
#include "Level/LevelChain.h"

using namespace o2;

namespace
{
	struct UserDataGuard
	{
		~UserDataGuard()
		{
			UserDataModel::Reset();
			LevelChain::Reset();
		}
	};
}

TEST(UserDataTests, WalletSpendsAndClamps)
{
	UserDataGuard guard;
	UserDataModel::Reset();

	int startCoins = UserDataModel::Get().coins;
	EXPECT_GT(startCoins, 0);
	EXPECT_GT(UserDataModel::Get().lives, 0);

	EXPECT_TRUE(UserDataModel::TrySpendCoins(10));
	EXPECT_EQ(UserDataModel::Get().coins, startCoins - 10);

	EXPECT_FALSE(UserDataModel::TrySpendCoins(startCoins));
	EXPECT_EQ(UserDataModel::Get().coins, startCoins - 10);

	EXPECT_FALSE(UserDataModel::TrySpendCoins(-1));

	UserDataModel::AddCoins(5);
	EXPECT_EQ(UserDataModel::Get().coins, startCoins - 5);

	UserDataModel::SetCoins(-100);
	EXPECT_EQ(UserDataModel::Get().coins, 0);

	UserDataModel::SetLives(-1);
	EXPECT_EQ(UserDataModel::Get().lives, 0);
}

TEST(UserDataTests, SoundSettingsPersistUntilReset)
{
	UserDataGuard guard;
	UserDataModel::Reset();

	EXPECT_TRUE(UserDataModel::Get().soundEnabled);
	EXPECT_TRUE(UserDataModel::Get().musicEnabled);

	UserDataModel::SetSoundEnabled(false);
	UserDataModel::SetMusicEnabled(false);
	EXPECT_FALSE(UserDataModel::Get().soundEnabled);
	EXPECT_FALSE(UserDataModel::Get().musicEnabled);

	UserDataModel::Reset();
	EXPECT_TRUE(UserDataModel::Get().soundEnabled);
	EXPECT_TRUE(UserDataModel::Get().musicEnabled);
}

TEST(UserDataTests, CurrentLevelClampsAndWraps)
{
	UserDataGuard guard;
	UserDataModel::Reset();
	LevelChain::Set({ "a.json", "b.json", "c.json" });

	UserDataModel::SetCurrentLevel(5, LevelChain::Count());
	EXPECT_EQ(UserDataModel::Get().currentLevel, 2);

	UserDataModel::SetCurrentLevel(-2, LevelChain::Count());
	EXPECT_EQ(UserDataModel::Get().currentLevel, 0);

	UserDataModel::AdvanceLevel(LevelChain::Count());
	EXPECT_EQ(UserDataModel::Get().currentLevel, 1);

	UserDataModel::AdvanceLevel(LevelChain::Count());
	UserDataModel::AdvanceLevel(LevelChain::Count());
	EXPECT_EQ(UserDataModel::Get().currentLevel, 0);

	// Empty chain leaves the index alone
	UserDataModel::AdvanceLevel(0);
	EXPECT_EQ(UserDataModel::Get().currentLevel, 0);
}

TEST(UserDataTests, SerializationRoundtrip)
{
	UserData data;
	data.lives = 3;
	data.coins = 42;
	data.soundEnabled = false;
	data.musicEnabled = true;
	data.currentLevel = 7;

	DataDocument doc;
	data.Serialize(doc);

	UserData restored;
	restored.Deserialize(doc);

	EXPECT_EQ(restored.lives, 3);
	EXPECT_EQ(restored.coins, 42);
	EXPECT_FALSE(restored.soundEnabled);
	EXPECT_TRUE(restored.musicEnabled);
	EXPECT_EQ(restored.currentLevel, 7);
}
