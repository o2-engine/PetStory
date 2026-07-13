#pragma once
#include "o2/Utils/Types/String.h"

using namespace o2;

// Fixed base of short Russian jokes shown in the victory window, one random pick per win
namespace Jokes
{
	int Count();
	const String& At(int index); // index is clamped into the base range
	const String& Random();
}
