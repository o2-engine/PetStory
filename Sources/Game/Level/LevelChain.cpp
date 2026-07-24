#include "o2/stdafx.h"
#include "Level/LevelChain.h"

#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/DataAsset.h"

namespace LevelChain
{
	static Vector<String> gChain;

	bool Load(const String& chainAssetPath)
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

		gChain = levels;
		return true;
	}

	void Set(const Vector<String>& levelPaths)
	{
		gChain = levelPaths;
	}

	int Count()
	{
		return gChain.Count();
	}

	String LevelPath(int index)
	{
		if (index < 0 || index >= gChain.Count())
			return String();

		return gChain[index];
	}

	void Reset()
	{
		gChain.Clear();
	}
}
