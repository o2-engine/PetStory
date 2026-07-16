#include "o2/stdafx.h"
#include "Localization.h"

namespace Loc
{
	static Lang gLanguage = Lang::Russian;

	void SetLanguage(Lang lang)
	{
		gLanguage = lang;
	}

	Lang GetLanguage()
	{
		return gLanguage;
	}

	void SetLanguageFromCode(const String& code)
	{
		if (code.IsEmpty())
			return;

		SetLanguage(code == "ru" ? Lang::Russian : Lang::English);
	}

	String Tr(const char* russian, const char* english)
	{
		return String(gLanguage == Lang::Russian ? russian : english);
	}
}
// --- META ---

ENUM_META(Loc::Lang, Loc__Lang)
{
    ENUM_ENTRY(English);
    ENUM_ENTRY(Russian);
}
END_ENUM_META;
// --- END META ---
