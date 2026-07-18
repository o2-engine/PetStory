#include "o2/stdafx.h"
#include "TicTacToeExport.h"

#include "o2/Animation/AnimationClip.h"
#include "o2/Animation/Tracks/AnimationSubTrack.h"
#include "o2/Animation/Tracks/AnimationTrack.h"
#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/AnimationAsset.h"
#include "o2/Assets/Types/SceneAsset.h"
#include "o2/EngineSettings.h"
#include "o2/Scene/Components/SoundComponent.h"
#include "o2/Scene/Scene.h"
#include "o2/Utils/Debug/Debug.h"

using namespace o2;

namespace TicTacToeExport
{
	namespace
	{
		void AddScaleKeys(const Ref<AnimationClip>& clip, const String& path,
						  std::initializer_list<Pair<float, float>> keys)
		{
			auto track = clip->AddTrack<Vec3F>(path);
			for (auto& key : keys)
				track->AddKey(key.first, Vec3F(key.second, key.second, 1.0f));
		}

		void AddSubTrack(const Ref<AnimationClip>& clip, const String& path, const Type& targetType)
		{
			auto track = DynamicCast<AnimationSubTrack>(clip->AddTrack(path, targetType));
			track->SetBeginTime(0.0f);
		}

		void SaveClip(const Ref<AnimationClip>& clip, const String& path)
		{
			AssetRef<AnimationAsset> asset;

			// Keep the existing UID on re-export so references in the saved scene stay valid
			if (o2Assets.GetAssetInfo(path).IsValid())
				asset = AssetRef<AnimationAsset>(path);

			if (!asset)
			{
				asset = AssetRef<AnimationAsset>::CreateAsset();
				asset->SetPath(path);
			}

			asset->animation = clip;
			asset->Save();

			o2Debug.Log("Exported animation: " + path);
		}
	}

	void ExportAnimations()
	{
		auto tokenSpawn = mmake<AnimationClip>();
		AddScaleKeys(tokenSpawn, "transform/scale", { { 0.0f, 0.01f }, { 0.18f, 1.15f }, { 0.3f, 1.0f } });
		SaveClip(tokenSpawn, "TicTacToe/Animations/TokenSpawn.anim");

		auto tokenPulse = mmake<AnimationClip>();
		tokenPulse->SetLoop(Loop::Repeat);
		AddScaleKeys(tokenPulse, "transform/scale", { { 0.0f, 1.0f }, { 0.4f, 1.08f }, { 0.8f, 1.0f } });
		SaveClip(tokenPulse, "TicTacToe/Animations/TokenPulse.anim");

		auto tokenFade = mmake<AnimationClip>();
		tokenFade->SetLoop(Loop::Repeat);
		auto fadeTrack = tokenFade->AddTrack<float>("component/o2::ImageComponent/transparency");
		fadeTrack->AddKey(0.0f, 0.85f);
		fadeTrack->AddKey(0.5f, 0.6f);
		fadeTrack->AddKey(1.0f, 0.85f);
		SaveClip(tokenFade, "TicTacToe/Animations/TokenFade.anim");

		auto avatarPulse = mmake<AnimationClip>();
		avatarPulse->SetLoop(Loop::Repeat);
		AddScaleKeys(avatarPulse, "transform/scale", { { 0.0f, 1.0f }, { 0.6f, 1.06f }, { 1.2f, 1.0f } });
		SaveClip(avatarPulse, "TicTacToe/Animations/AvatarPulse.anim");

		auto strikeGrow = mmake<AnimationClip>();
		AddScaleKeys(strikeGrow, "transform/scale", { { 0.0f, 0.002f }, { 0.15f, 0.72f }, { 0.35f, 1.0f } });
		AddSubTrack(strikeGrow, "component/o2::SoundComponent", TypeOf(SoundComponent));
		SaveClip(strikeGrow, "TicTacToe/Animations/StrikeGrow.anim");

		auto windowWin = mmake<AnimationClip>();
		AddScaleKeys(windowWin, "child/Panel/transform/scale", { { 0.0f, 0.6f }, { 0.21f, 1.06f }, { 0.3f, 1.0f } });
		AddSubTrack(windowWin, "child/WinSound/component/o2::SoundComponent", TypeOf(SoundComponent));
		SaveClip(windowWin, "TicTacToe/Animations/WindowWin.anim");

		auto windowLose = mmake<AnimationClip>();
		AddScaleKeys(windowLose, "child/Panel/transform/scale", { { 0.0f, 0.6f }, { 0.21f, 1.06f }, { 0.3f, 1.0f } });
		AddSubTrack(windowLose, "child/LoseSound/component/o2::SoundComponent", TypeOf(SoundComponent));
		SaveClip(windowLose, "TicTacToe/Animations/WindowLose.anim");

		// Particles replay baked frames when driven by a sub-track (an editor-preview path),
		// so emitters are played directly from script; the clip only fires the pop sound
		auto poofFx = mmake<AnimationClip>();
		AddSubTrack(poofFx, "component/o2::SoundComponent", TypeOf(SoundComponent));
		SaveClip(poofFx, "TicTacToe/Animations/PoofFx.anim");
	}

	void ExportScene()
	{
		const String scenePath = "TicTacToe.scn";
		String fullPath = String(::GetAssetsPath()) + scenePath;

		if (o2Assets.GetAssetInfo(scenePath).IsValid())
		{
			o2Scene.Save(fullPath);
		}
		else
		{
			auto sceneAsset = AssetRef<SceneAsset>::CreateAsset();
			sceneAsset->SetPath(scenePath);
			sceneAsset->Save();
		}

		o2Debug.Log("Exported scene: " + fullPath);
	}
}
