#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "o2/Render/Render.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Components/AnimationComponent.h"
#include "o2/Scene/Components/ParticlesEmitterComponent.h"
#include "o2/Scene/Components/ScriptableComponent.h"
#include "o2/Scene/Components/SkinnedMeshComponent.h"
#include "o2/Scene/Components/SoundComponent.h"
#include "o2/Scene/Scene.h"
#include "o2/Sound/SoundSystem.h"
#include "o2/Utils/Bitmap/Bitmap.h"
#include "o2/Utils/Test/AppTestDriver.h"

#include "PipelineDemoScene.h"

using namespace o2;

namespace
{
	const String kScreenshotsDir = "TestScreenshots/";

	int CountDistinctColors(const Ref<Bitmap>& bitmap)
	{
		if (!bitmap)
			return 0;

		Vector<UInt32> seen;
		const UInt32* pixels = reinterpret_cast<const UInt32*>(bitmap->GetData());
		Vec2I size = bitmap->GetSize();
		for (int y = 0; y < size.y; y += 16)
		{
			for (int x = 0; x < size.x; x += 16)
			{
				UInt32 color = pixels[y*size.x + x];
				if (!seen.Contains(color))
					seen.Add(color);
			}
		}

		return seen.Count();
	}

	// y is in top-down image coordinates; the bitmap stores rows bottom-up
	const UInt8* GetPixel(const Ref<Bitmap>& bitmap, int x, int y)
	{
		Vec2I size = bitmap->GetSize();
		x = Math::Clamp(x, 0, size.x - 1);
		y = Math::Clamp(y, 0, size.y - 1);
		return bitmap->GetData() + ((size.y - 1 - y)*size.x + x)*4;
	}
}

class PipelineDemoUI: public ::testing::Test
{
protected:
	Ref<CameraActor> camera;
	Ref<CameraActor> uiCamera;

	void SetUp() override
	{
		camera = BuildPipelineDemoScene();
		ASSERT_TRUE(camera);

		AppTestDriver::PumpFrames(5); // settle transforms and prime the cameras

		for (auto& weakCamera : o2Scene.GetCameras())
		{
			auto sceneCamera = weakCamera.Lock();
			if (sceneCamera && sceneCamera->GetName() == "ui camera")
				uiCamera = sceneCamera;
		}

		ASSERT_TRUE(uiCamera);
	}

	void TearDown() override
	{
		camera = nullptr;
		uiCamera = nullptr;

		o2Scene.Clear(true);
		o2Scene.UpdateDestroyingEntities();
		AppTestDriver::PumpFrames(2);
	}
};

// The composed frame shows the lit 3D scene with the 2D layer (sprites, UI) drawn on top
TEST_F(PipelineDemoUI, ComposedFrameShows3DAnd2DLayers)
{
	Ref<Bitmap> bitmap = AppTestDriver::TakeScreenshot();
	ASSERT_TRUE(bitmap);
	EXPECT_GT(CountDistinctColors(bitmap), 12) << "the lit 3D demo scene is far from a blank frame";

	// The deferred lit 3D content fills the center (the normal mapped box is there)
	Vec2I size = bitmap->GetSize();
	const UInt8* center = GetPixel(bitmap, size.x/2, size.y/2);
	int centerBrightness = (int)center[0] + center[1] + center[2];
	EXPECT_GT(centerBrightness, 60) << "the lit 3D content must be visible at screen center";

	// 2D sprites live in the top-left area: the red chip must be drawn over the 3D image
	Vec2F chipScreen = uiCamera->listenersLayer->ScreenFromLocal(Vec2F(-560, 430));
	Vec2F screenCenter = (Vec2F)o2Render.GetResolution()*0.5f;
	Vec2I chipPixel((int)(chipScreen.x + screenCenter.x), (int)(screenCenter.y - chipScreen.y));

	bool foundRed = false;
	for (int dy = -10; dy <= 10 && !foundRed; dy += 2)
	{
		for (int dx = -10; dx <= 10 && !foundRed; dx += 2)
		{
			const UInt8* pixel = GetPixel(bitmap, chipPixel.x + dx, chipPixel.y + dy);
			if (pixel[0] > 150 && pixel[1] < 110 && pixel[2] < 110)
				foundRed = true;
		}
	}

	EXPECT_TRUE(foundRed) << "the red chip sprite from the 2D layer must overlay the 3D image at "
		<< chipPixel.x << ", " << chipPixel.y;

	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "pipeline_demo.png"));
}

// Demo sounds resolve their assets, decode real durations and keep playing in loop
TEST_F(PipelineDemoUI, DemoSoundsLoadedAndPlaying)
{
	ASSERT_TRUE(o2Sounds.IsReady());

	Map<String, float> expectedDurations{ { "water sound", 18.8f }, { "cat meow", 1.5f }, { "birds ambience", 42.7f } };
	for (auto& kv : expectedDurations)
	{
		auto actor = o2Scene.FindActor(kv.first);
		ASSERT_TRUE(actor) << (const char*)kv.first;

		auto sound = actor->GetComponent<SoundComponent>();
		ASSERT_TRUE(sound) << (const char*)kv.first;

		EXPECT_NEAR(sound->GetDuration(), kv.second, 0.2f) << (const char*)kv.first;
	}

	for (auto& name : { "water sound", "birds ambience" })
	{
		auto sound = o2Scene.FindActor(name)->GetComponent<SoundComponent>();
		EXPECT_TRUE(sound->IsPlaying()) << name;
		EXPECT_EQ(sound->GetLoop(), Loop::Repeat) << name;
	}

	auto meow = o2Scene.FindActor("cat meow")->GetComponent<SoundComponent>();
	EXPECT_TRUE(meow->IsSpatial());
	EXPECT_FALSE(o2Scene.FindActor("birds ambience")->GetComponent<SoundComponent>()->IsSpatial());

	// The meow is driven by the animation clip sub track instead of direct playback
	EXPECT_TRUE(meow->IsSubControlled());
	EXPECT_FALSE(meow->IsPlaying());

	// Spatial source position follows the actor transform
	auto meowPosition = meow->GetPosition();
	EXPECT_NEAR(meowPosition.x, -80.0f, 0.1f);
	EXPECT_NEAR(meowPosition.y, -220.0f, 0.1f);

	// Time advances while frames are pumped
	float timeBefore = o2Scene.FindActor("water sound")->GetComponent<SoundComponent>()->GetTime();
	AppTestDriver::PumpFrames(10);
	EXPECT_GT(o2Scene.FindActor("water sound")->GetComponent<SoundComponent>()->GetTime(), timeBefore);
}

// Both skinned characters are animated through bone actors + AnimationComponent:
// frames captured at different times differ in the characters areas
TEST_F(PipelineDemoUI, SkinnedCharactersAnimate)
{
	auto fox = o2Scene.FindActor("fox");
	ASSERT_TRUE(fox);

	auto foxMesh = fox->GetComponent<SkinnedMeshComponent>();
	ASSERT_TRUE(foxMesh);
	EXPECT_TRUE(foxMesh->IsUsingBoneActors());
	ASSERT_TRUE(fox->GetComponent<AnimationComponent>());
	EXPECT_TRUE(fox->GetComponent<AnimationComponent>()->GetState("Run"));
	EXPECT_FALSE(fox->GetChildren().IsEmpty()) << "fox bone actors must be created in the hierarchy";

	auto cesiumMan = o2Scene.FindActor("cesium man");
	ASSERT_TRUE(cesiumMan);
	ASSERT_TRUE(cesiumMan->GetComponent<SkinnedMeshComponent>());
	EXPECT_TRUE(cesiumMan->GetComponent<SkinnedMeshComponent>()->IsUsingBoneActors());
	ASSERT_TRUE(cesiumMan->GetComponent<AnimationComponent>());
	EXPECT_FALSE(cesiumMan->GetChildren().IsEmpty()) << "cesium man bone actors must be created in the hierarchy";

	Ref<Bitmap> frameA = AppTestDriver::TakeScreenshot();
	ASSERT_TRUE(frameA);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "pipeline_demo_fox_a.png"));

	AppTestDriver::PumpFrames(20);

	Ref<Bitmap> frameB = AppTestDriver::TakeScreenshot();
	ASSERT_TRUE(frameB);
	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "pipeline_demo_fox_b.png"));

	auto countDifferentInRect = [&](const Vec2I& min, const Vec2I& max)
	{
		int different = 0;
		for (int y = min.y; y < max.y; y += 4)
		{
			for (int x = min.x; x < max.x; x += 4)
			{
				const UInt8* pa = GetPixel(frameA, x, y);
				const UInt8* pb = GetPixel(frameB, x, y);
				if (Math::Abs((int)pa[0] - pb[0]) + Math::Abs((int)pa[1] - pb[1]) +
					Math::Abs((int)pa[2] - pb[2]) > 60)
				{
					different++;
				}
			}
		}

		return different;
	};

	Vec2I size = frameA->GetSize();
	int different = countDifferentInRect(Vec2I(), size);
	EXPECT_GT(different, 30) << "characters animation must visibly change the frame, changed samples: " << different;

	// Both halves of the 3D content change: the fox is at the left, the cesium man at the right
	int leftDifferent = countDifferentInRect(Vec2I(), Vec2I(size.x/2, size.y));
	int rightDifferent = countDifferentInRect(Vec2I(size.x/2, 0), size);
	EXPECT_GT(leftDifferent, 10) << "fox animation must change the left half of the frame";
	EXPECT_GT(rightDifferent, 10) << "cesium man animation must change the right half of the frame";
}

// 2D and 3D particles emitters live on their layers and keep emitting alive particles
TEST_F(PipelineDemoUI, ParticlesEmittersWorkIn2DAnd3D)
{
	auto sparks = o2Scene.FindActor("3d sparks");
	ASSERT_TRUE(sparks);
	auto sparksEmitter = sparks->GetComponent<ParticlesEmitterComponent>();
	ASSERT_TRUE(sparksEmitter);
	EXPECT_TRUE(sparksEmitter->Is3D());
	EXPECT_EQ(sparksEmitter->GetSceneDrawableCategory(), SceneDrawableCategory::Scene3D);

	auto confetti = o2Scene.FindActor("2d confetti");
	ASSERT_TRUE(confetti);
	auto confettiEmitter = confetti->GetComponent<ParticlesEmitterComponent>();
	ASSERT_TRUE(confettiEmitter);
	EXPECT_FALSE(confettiEmitter->Is3D());
	EXPECT_EQ(confettiEmitter->GetSceneDrawableCategory(), SceneDrawableCategory::Scene2D);

	AppTestDriver::PumpFrames(10);

	EXPECT_GT(sparksEmitter->GetParticlesCount(), 0) << "3d sparks must have alive particles";
	EXPECT_GT(confettiEmitter->GetParticlesCount(), 0) << "2d confetti must have alive particles";

	// 3D particles move in world space along the fountain cone
	bool hasVerticalMotion = false;
	for (auto& particle : sparksEmitter->GetParticles())
	{
		if (particle.alive && Math::Abs(particle.velocity.z) > 0.01f)
		{
			hasVerticalMotion = true;
			break;
		}
	}
	EXPECT_TRUE(hasVerticalMotion) << "3d sparks must move along z axis";

	EXPECT_TRUE(AppTestDriver::SaveScreenshot(kScreenshotsDir + "pipeline_demo_particles.png"));
}

// Rotator.js scriptable component drives the tilted box: the JS class is loaded from
// the Scripts/Rotator.js asset and its Update spins the actor around Z every frame
TEST_F(PipelineDemoUI, RotatorScriptSpinsTiltedBox)
{
	auto box = o2Scene.FindActor("tilted box");
	ASSERT_TRUE(box);

	auto scriptable = box->GetComponent<ScriptableComponent>();
	ASSERT_TRUE(scriptable);
	ASSERT_TRUE(scriptable->GetInstance().IsObject()) << "Rotator script class must be loaded from assets";
	EXPECT_FLOAT_EQ(scriptable->GetInstance().GetProperty("speed").ToNumber(), 0.8f);

	float angleBefore = box->transform->GetAngle();
	AppTestDriver::PumpFrames(10);

	EXPECT_GT(box->transform->GetAngle(), angleBefore) << "Rotator.js Update must rotate the box";
}
