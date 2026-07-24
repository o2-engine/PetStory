#pragma once

#include "o2/Utils/Function/Function.h"
#include "o2/Utils/Types/Containers/Map.h"
#include "o2/Utils/Types/String.h"

using namespace o2;

// ------------------------------------------------------------------
// Simple localization: a flat key -> text table loaded from a JSON
// data asset ({ "key": "text", ... }). LocalizedTextComponent pulls
// label texts from here and refreshes on language change.
// ------------------------------------------------------------------
namespace Localization
{
	// Loads the strings table from the data asset; false when missing or empty
	bool LoadLanguage(const String& assetPath);

	// Returns the asset path of the loaded language, empty when none
	const String& GetLanguageAssetPath();

	// Returns localized text by key; the key itself when missing
	WString GetText(const String& key);

	bool HasText(const String& key);

	// Returns localized text with {param} placeholders replaced
	WString Format(const String& key, const Map<String, WString>& params);

	// Fired after every successful LoadLanguage
	Function<void()>& OnLanguageChanged();

	// Forgets the loaded table
	void Reset();
}
