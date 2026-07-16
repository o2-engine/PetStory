#include "o2/stdafx.h"
#include "YandexGames.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace YandexGames
{
	// -1 = nothing pending; written by the JS ad callbacks (wasm) or by the stub (other platforms),
	// consumed once by PopRewardedResult on the game loop
	static int gPendingRewardedResult = -1;
}

#ifdef __EMSCRIPTEN__

extern "C" EMSCRIPTEN_KEEPALIVE void o2_OnYandexRewardedResult(int rewarded);

void o2_OnYandexRewardedResult(int rewarded)
{
	YandexGames::gPendingRewardedResult = rewarded;
}

EM_JS(char*, yg_get_language, (), {
	var lang = '';
	try
	{
		if (window.ysdk && window.ysdk.environment && window.ysdk.environment.i18n)
			lang = window.ysdk.environment.i18n.lang || '';
	}
	catch (e) {}

	var size = lengthBytesUTF8(lang) + 1;
	var buffer = _malloc(size);
	stringToUTF8(lang, buffer, size);
	return buffer;
});

EM_JS(void, yg_show_rewarded_video, (), {
	if (!(window.ysdk && window.ysdk.adv && window.ysdk.adv.showRewardedVideo))
	{
		console.warn('[ysdk] rewarded video requested without the SDK, reporting failure');
		_o2_OnYandexRewardedResult(0);
		return;
	}

	var rewarded = false;
	window.ysdk.adv.showRewardedVideo({
		callbacks: {
			onRewarded: function() { rewarded = true; },
			onClose: function() { _o2_OnYandexRewardedResult(rewarded ? 1 : 0); },
			onError: function(e) { console.error('[ysdk] rewarded video error', e); _o2_OnYandexRewardedResult(0); }
		}
	});
});

EM_JS(void, yg_notify_ready, (), {
	try
	{
		if (window.ysdk && window.ysdk.features && window.ysdk.features.LoadingAPI)
			window.ysdk.features.LoadingAPI.ready();
	}
	catch (e) {}
});

namespace YandexGames
{
	String GetLanguage()
	{
		char* lang = yg_get_language();
		String result(lang);
		free(lang);
		return result;
	}

	void ShowRewardedVideo()
	{
		gPendingRewardedResult = -1;
		yg_show_rewarded_video();
	}

	void NotifyReady()
	{
		yg_notify_ready();
	}
}

#else

namespace YandexGames
{
	String GetLanguage()
	{
		return String();
	}

	void ShowRewardedVideo()
	{
		gPendingRewardedResult = 1;
	}

	void NotifyReady()
	{}
}

#endif

namespace YandexGames
{
	int PopRewardedResult()
	{
		int result = gPendingRewardedResult;
		gPendingRewardedResult = -1;
		return result;
	}
}
