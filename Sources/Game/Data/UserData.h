#pragma once

#include "o2/Utils/Serialization/Serializable.h"

using namespace o2;

// ------------------------------------------------------------------
// User profile data: plain state without any change logic, mutated
// only through UserDataModel.
// ------------------------------------------------------------------
struct UserData: public ISerializable
{
	int  lives = 5;           // @SERIALIZABLE
	int  coins = 100;         // @SERIALIZABLE
	bool soundEnabled = true; // @SERIALIZABLE
	bool musicEnabled = true; // @SERIALIZABLE
	int  currentLevel = 0;    // @SERIALIZABLE Current level index in the chain

	SERIALIZABLE(UserData);
};
// --- META ---

CLASS_BASES_META(UserData)
{
    BASE_CLASS(ISerializable);
}
END_META;
CLASS_FIELDS_META(UserData)
{
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(5).NAME(lives);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(100).NAME(coins);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(true).NAME(soundEnabled);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(true).NAME(musicEnabled);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0).NAME(currentLevel);
}
END_META;
CLASS_METHODS_META(UserData)
{
}
END_META;
// --- END META ---
