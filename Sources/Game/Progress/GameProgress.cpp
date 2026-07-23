#include "o2/stdafx.h"
#include "Progress/GameProgress.h"

#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/DataAsset.h"

namespace GameProgress
{
	static Vector<String> gChain;
	static int gCurrentLevel = 0;

	bool LoadChain(const String& chainAssetPath)
	{
		Vector<String> levels;

		AssetRef<DataAsset> asset(chainAssetPath);
		if (asset)
		{
			if (auto levelsNode = asset->data.FindMember("levels"); levelsNode && levelsNode->IsArray())
			{
				for (auto& element : *levelsNode)
				{
					String path;
					element.Get(path);
					if (!path.IsEmpty())
						levels.Add(path);
				}
			}
		}

		if (levels.IsEmpty())
			return false;

		SetChain(levels);
		return true;
	}

	void SetChain(const Vector<String>& levelPaths)
	{
		gChain = levelPaths;
		gCurrentLevel = Math::Clamp(gCurrentLevel, 0, Math::Max(0, gChain.Count() - 1));
	}

	int GetLevelsCount()
	{
		return gChain.Count();
	}

	int GetCurrentLevel()
	{
		return gCurrentLevel;
	}

	void SetCurrentLevel(int index)
	{
		gCurrentLevel = Math::Clamp(index, 0, Math::Max(0, gChain.Count() - 1));
	}

	String GetCurrentLevelPath()
	{
		if (gChain.IsEmpty())
			return String();

		return gChain[gCurrentLevel];
	}

	void AdvanceLevel()
	{
		if (gChain.IsEmpty())
			return;

		gCurrentLevel = (gCurrentLevel + 1) % gChain.Count();
	}

	void Reset()
	{
		gChain.Clear();
		gCurrentLevel = 0;
	}
}
