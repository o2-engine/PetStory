#pragma once

#include "o2/Utils/Math/Vector2.h"
#include "o2/Utils/Serialization/Serializable.h"
#include "o2/Utils/Types/Containers/Vector.h"
#include "o2/Utils/Types/String.h"

using namespace o2;

// One level goal: collect `count` chips of color `chipType` (Blue/Green/Orange/Red/Violet/Yellow)
struct LevelGoal: public ISerializable
{
	String chipType; // @SERIALIZABLE
	int    count = 0; // @SERIALIZABLE

	bool operator==(const LevelGoal& other) const;

	SERIALIZABLE(LevelGoal);
};

// Chip spawn point: keeps up to maxOnScreen chips of the listed colors alive,
// dropping new ones into the zone rect centered at position
struct LevelSpawnPoint: public ISerializable
{
	Vec2F          position;            // @SERIALIZABLE
	Vec2F          zoneSize = Vec2F(400.0f, 100.0f); // @SERIALIZABLE
	Vector<String> colors;              // @SERIALIZABLE
	int            maxOnScreen = 10;    // @SERIALIZABLE
	float          spawnDelay = 0.2f;   // @SERIALIZABLE

	bool operator==(const LevelSpawnPoint& other) const;

	SERIALIZABLE(LevelSpawnPoint);
};

// Inner wall strip (shelf or tunnel side): polyline in field space
struct LevelWall: public ISerializable
{
	Vector<Vec2F> points;         // @SERIALIZABLE
	float         width = 36.0f;  // @SERIALIZABLE
	bool          closed = false; // @SERIALIZABLE

	bool operator==(const LevelWall& other) const;

	SERIALIZABLE(LevelWall);
};

// ------------------------------------------------------------------
// Lightweight level description, stored as a plain JSON DataAsset.
// border is the closed field outline; walls add inner shelves and
// tunnels; spawners define chip sources; goals - up to kMaxGoals.
// ------------------------------------------------------------------
struct LevelData: public ISerializable
{
	static constexpr int kMaxGoals = 4;

	String                  name;      // @SERIALIZABLE
	Vector<Vec2F>           border;    // @SERIALIZABLE
	Vector<LevelWall>       walls;     // @SERIALIZABLE
	Vector<LevelSpawnPoint> spawners;  // @SERIALIZABLE
	Vector<LevelGoal>       goals;     // @SERIALIZABLE
	int                     moves = 0; // @SERIALIZABLE Moves limit, 0 - unlimited

	bool operator==(const LevelData& other) const;

	// Clamps goals to kMaxGoals after load
	void OnDeserialized(const DataValue& node) override;

	// Loads level from a JSON data asset by path, e.g. "Levels/Level01.json".
	// Returns false and leaves *this default when the asset is missing or empty
	bool LoadFromAsset(const String& assetPath);

	SERIALIZABLE(LevelData);
};
// --- META ---

CLASS_BASES_META(LevelGoal)
{
    BASE_CLASS(ISerializable);
}
END_META;
CLASS_FIELDS_META(LevelGoal)
{
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(chipType);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0).NAME(count);
}
END_META;
CLASS_METHODS_META(LevelGoal)
{
}
END_META;

CLASS_BASES_META(LevelSpawnPoint)
{
    BASE_CLASS(ISerializable);
}
END_META;
CLASS_FIELDS_META(LevelSpawnPoint)
{
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(position);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(Vec2F(400.0f, 100.0f)).NAME(zoneSize);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(colors);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(10).NAME(maxOnScreen);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0.2f).NAME(spawnDelay);
}
END_META;
CLASS_METHODS_META(LevelSpawnPoint)
{
}
END_META;

CLASS_BASES_META(LevelWall)
{
    BASE_CLASS(ISerializable);
}
END_META;
CLASS_FIELDS_META(LevelWall)
{
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(points);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(36.0f).NAME(width);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(false).NAME(closed);
}
END_META;
CLASS_METHODS_META(LevelWall)
{
}
END_META;

CLASS_BASES_META(LevelData)
{
    BASE_CLASS(ISerializable);
}
END_META;
CLASS_FIELDS_META(LevelData)
{
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(name);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(border);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(walls);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(spawners);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().NAME(goals);
    FIELD().PUBLIC().SERIALIZABLE_ATTRIBUTE().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(0).NAME(moves);
}
END_META;
CLASS_METHODS_META(LevelData)
{

    FUNCTION().PUBLIC().SIGNATURE(void, OnDeserialized, const DataValue&);
    FUNCTION().PUBLIC().SIGNATURE(bool, LoadFromAsset, const String&);
}
END_META;
// --- END META ---
