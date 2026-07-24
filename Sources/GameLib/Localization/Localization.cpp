#include "o2/stdafx.h"
#include "GameLib/Localization/Localization.h"

#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/DataAsset.h"

namespace Localization
{
	static Map<String, WString> gTexts;
	static String gLanguagePath;
	static Function<void()> gOnLanguageChanged;

	bool LoadLanguage(const String& assetPath)
	{
		AssetRef<DataAsset> asset(assetPath);
		if (!asset)
			return false;

		Map<String, WString> texts;
		for (auto element = asset->data.BeginMember(); element != asset->data.EndMember(); ++element)
		{
			String key;
			element->name.Get(key);

			WString text;
			element->value.Get(text);

			if (!key.IsEmpty())
				texts[key] = text;
		}

		if (texts.IsEmpty())
			return false;

		gTexts = texts;
		gLanguagePath = assetPath;
		gOnLanguageChanged();
		return true;
	}

	const String& GetLanguageAssetPath()
	{
		return gLanguagePath;
	}

	WString GetText(const String& key)
	{
		WString result;
		if (gTexts.TryGetValue(key, result))
			return result;

		return WString(key);
	}

	bool HasText(const String& key)
	{
		return gTexts.ContainsKey(key);
	}

	WString Format(const String& key, const Map<String, WString>& params)
	{
		WString result = GetText(key);
		for (auto& kv : params)
			result.ReplaceAll(WString(String("{") + kv.first + "}"), kv.second);

		return result;
	}

	Function<void()>& OnLanguageChanged()
	{
		return gOnLanguageChanged;
	}

	void Reset()
	{
		gTexts.Clear();
		gLanguagePath = String();
	}
}
