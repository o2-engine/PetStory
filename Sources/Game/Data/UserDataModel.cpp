#include "o2/stdafx.h"
#include "Data/UserDataModel.h"

namespace UserDataModel
{
	static UserData gData;

	UserData& Get()
	{
		return gData;
	}

	void Reset()
	{
		gData = UserData();
	}

	void SetLives(int count)
	{
		gData.lives = Math::Max(0, count);
	}

	void SetCoins(int count)
	{
		gData.coins = Math::Max(0, count);
	}

	void AddCoins(int amount)
	{
		gData.coins = Math::Max(0, gData.coins + amount);
	}

	bool TrySpendCoins(int amount)
	{
		if (amount < 0 || gData.coins < amount)
			return false;

		gData.coins -= amount;
		return true;
	}

	void SetSoundEnabled(bool enabled)
	{
		gData.soundEnabled = enabled;
	}

	void SetMusicEnabled(bool enabled)
	{
		gData.musicEnabled = enabled;
	}

	void SetCurrentLevel(int index, int levelsCount)
	{
		gData.currentLevel = Math::Clamp(index, 0, Math::Max(0, levelsCount - 1));
	}

	void AdvanceLevel(int levelsCount)
	{
		if (levelsCount <= 0)
			return;

		gData.currentLevel = (gData.currentLevel + 1) % levelsCount;
	}
}
