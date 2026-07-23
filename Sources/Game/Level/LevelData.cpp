#include "o2/stdafx.h"
#include "Level/LevelData.h"

#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/DataAsset.h"

bool LevelGoal::operator==(const LevelGoal& other) const
{
	return chipType == other.chipType && count == other.count;
}

bool LevelSpawnPoint::operator==(const LevelSpawnPoint& other) const
{
	return position == other.position && zoneSize == other.zoneSize && colors == other.colors &&
		maxOnScreen == other.maxOnScreen && Math::Equals(spawnDelay, other.spawnDelay);
}

bool LevelWall::operator==(const LevelWall& other) const
{
	return points == other.points && Math::Equals(width, other.width) && closed == other.closed;
}

bool LevelData::operator==(const LevelData& other) const
{
	return name == other.name && border == other.border && walls == other.walls &&
		spawners == other.spawners && goals == other.goals;
}

void LevelData::OnDeserialized(const DataValue& node)
{
	if (goals.Count() > kMaxGoals)
		goals.Resize(kMaxGoals);
}

bool LevelData::LoadFromAsset(const String& assetPath)
{
	*this = LevelData();

	AssetRef<DataAsset> asset(assetPath);
	if (!asset)
		return false;

	Deserialize(asset->data);
	return !border.IsEmpty();
}
// --- META ---

DECLARE_CLASS(LevelGoal, LevelGoal);

DECLARE_CLASS(LevelSpawnPoint, LevelSpawnPoint);

DECLARE_CLASS(LevelWall, LevelWall);

DECLARE_CLASS(LevelData, LevelData);
// --- END META ---
