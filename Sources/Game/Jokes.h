#pragma once
#include "o2/Utils/Types/String.h"

using namespace o2;

// Fixed base of short jokes shown in the victory window, one random pick per win;
// each joke is authored in Russian and English, returned in the current Loc language
namespace Jokes
{
	int Count();
	String At(int index); // index is clamped into the base range
	String Random();
}
