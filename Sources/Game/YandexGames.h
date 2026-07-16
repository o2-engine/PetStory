#pragma once
#include "o2/Utils/Types/String.h"

using namespace o2;

// Thin wrapper over the Yandex Games JS SDK. On WebAssembly it talks to window.ysdk (initialized
// by the HTML shell before main() runs); on other platforms it degrades gracefully: no language
// (the game keeps its default) and rewarded videos "succeed" immediately so the flow stays playable.
namespace YandexGames
{
	// The SDK environment language (ISO 639-1, e.g. "ru"); empty when the SDK is unavailable
	String GetLanguage();

	// Starts a rewarded video. The result is delivered asynchronously through PopRewardedResult;
	// any not-yet-consumed result of a previous show is dropped.
	void ShowRewardedVideo();

	// Returns and clears the pending rewarded result: 1 rewarded, 0 closed or failed, -1 none yet
	int PopRewardedResult();

	// Reports the game is loaded and interactive (LoadingAPI.ready), required by Yandex Games
	void NotifyReady();
}
