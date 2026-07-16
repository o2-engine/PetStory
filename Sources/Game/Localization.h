#pragma once
#include "o2/Utils/Types/String.h"

using namespace o2;

// Minimal two-language string table: every user-facing string is authored in place as a
// (russian, english) pair, picked by the language set once at startup from the platform SDK
namespace Loc
{
	enum class Lang { Russian, English };

	void SetLanguage(Lang lang);
	Lang GetLanguage();

	// Maps an ISO 639-1 code to a supported language: "ru" -> Russian, any other non-empty
	// code -> English; an empty code (no SDK) keeps the current language
	void SetLanguageFromCode(const String& code);

	// Returns the variant matching the current language
	String Tr(const char* russian, const char* english);
}
// --- META ---

PRE_ENUM_META(Loc::Lang);
// --- END META ---
