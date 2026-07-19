#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "o2/Render/Render.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Scene/Components/ScriptableComponent.h"
#include "o2/Scene/Scene.h"
#include "o2/Utils/Bitmap/Bitmap.h"
#include "o2/Utils/FileSystem/FileSystem.h"
#include "o2/Utils/Test/AppTestDriver.h"

#include "ChipLitDemoScene.h"

using namespace o2;

namespace
{
	const String kScreenshotsDir = "TestScreenshots/";

	// y is in top-down image coordinates; the bitmap stores rows bottom-up
	const UInt8* GetPixel(const Ref<Bitmap>& bitmap, int x, int y)
	{
		Vec2I size = bitmap->GetSize();
		x = Math::Clamp(x, 0, size.x - 1);
		y = Math::Clamp(y, 0, size.y - 1);
		return bitmap->GetData() + ((size.y - 1 - y)*size.x + x)*4;
	}

	int CountDifferentInRect(const Ref<Bitmap>& a, const Ref<Bitmap>& b, const Vec2I& min, const Vec2I& max)
	{
		int different = 0;
		for (int y = min.y; y < max.y; y += 3)
		{
			for (int x = min.x; x < max.x; x += 3)
			{
				const UInt8* pa = GetPixel(a, x, y);
				const UInt8* pb = GetPixel(b, x, y);
				if (Math::Abs((int)pa[0] - pb[0]) + Math::Abs((int)pa[1] - pb[1]) +
					Math::Abs((int)pa[2] - pb[2]) > 60)
				{
					different++;
				}
			}
		}

		return different;
	}
}

class ChipLitDemoUI: public ::testing::Test
{
protected:
	Ref<CameraActor> camera;

	void SetUp() override
	{
		camera = BuildChipLitDemoScene();
		ASSERT_TRUE(camera);
		AppTestDriver::PumpFrames(5);
	}

	void TearDown() override
	{
		camera = nullptr;
		o2Scene.Clear(true);
		o2Scene.UpdateDestroyingEntities();
		AppTestDriver::PumpFrames(2);
	}

	Ref<Bitmap> Capture()
	{
		Ref<Bitmap> bitmap = AppTestDriver::TakeScreenshot();
		return bitmap;
	}

	Vec2I WorldToPixel(const Ref<Bitmap>& bitmap, const Vec2F& worldPos)
	{
		Vec2F screen = camera->listenersLayer->ScreenFromLocal(worldPos);
		Vec2F resolution = (Vec2F)o2Render.GetResolution();
		Vec2F bitmapSize = (Vec2F)bitmap->GetSize();
		Vec2F center = resolution*0.5f;
		return Vec2I((int)((screen.x + center.x)*bitmapSize.x/resolution.x),
		             (int)((center.y - screen.y)*bitmapSize.y/resolution.y));
	}
};

// The demo scene builds: three spinning lit chips with the chip_lit materials and the
// ChipSpin.js script, three static reference sprites beside them
TEST_F(ChipLitDemoUI, SceneBuildsWithSpinningLitChips)
{
	for (auto name : { "red", "blue", "leaf" })
	{
		auto lit = o2Scene.FindActor(String("lit ") + name);
		ASSERT_TRUE(lit) << name;

		auto image = lit->GetComponent<ImageComponent>();
		ASSERT_TRUE(image) << name;
		ASSERT_TRUE(image->GetMaterial()) << name;
		EXPECT_TRUE(image->GetMaterial()->IsReady()) << name;

		auto spin = lit->GetComponent<ScriptableComponent>();
		ASSERT_TRUE(spin) << name;
		ASSERT_TRUE(spin->GetInstance().IsObject()) << "ChipSpin script class must be loaded, " << name;

		EXPECT_TRUE(o2Scene.FindActor(String("ref ") + name)) << name;
	}

	// The script actually spins: the angle changes over pumped frames
	auto litRed = o2Scene.FindActor("lit red");
	float angleBefore = litRed->transform->GetAngle();
	AppTestDriver::PumpFrames(10);
	EXPECT_NE(litRed->transform->GetAngle(), angleBefore) << "ChipSpin.js must rotate the chip";
}

// Spinning changes the lit chips between frames while the reference chips stay identical,
// and the lighting stays anchored: the bright side of the lit balls remains up in both frames
TEST_F(ChipLitDemoUI, ChipsSpinLightStaysReferencesStill)
{
	Ref<Bitmap> frameA = Capture();
	ASSERT_TRUE(frameA);
	AppTestDriver::Wait(0.75f); // real time: the spin speeds are radians per second
	Ref<Bitmap> frameB = Capture();
	ASSERT_TRUE(frameB);

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	EXPECT_TRUE(frameA->Save(kScreenshotsDir + "chip_lit_demo_a.png", Bitmap::ImageType::Png));
	EXPECT_TRUE(frameB->Save(kScreenshotsDir + "chip_lit_demo_b.png", Bitmap::ImageType::Png));

	struct Probe { const char* name; Vec2F worldPos; float radius; };
	const Probe probes[] = {
		{ "lit red",  Vec2F(-280, 320),  105.0f },
		{ "lit blue", Vec2F(-280, 0),    105.0f },
		{ "lit leaf", Vec2F(-280, -330), 100.0f },
		{ "ref red",  Vec2F(280, 320),   105.0f },
		{ "ref blue", Vec2F(280, 0),     105.0f },
		{ "ref leaf", Vec2F(280, -330),  100.0f },
	};

	for (auto& probe : probes)
	{
		Vec2I center = WorldToPixel(frameA, probe.worldPos);
		Vec2F resolution = (Vec2F)o2Render.GetResolution();
		int radius = (int)(probe.radius*(float)frameA->GetSize().x/resolution.x);
		Vec2I min(center.x - radius, center.y - radius);
		Vec2I max(center.x + radius, center.y + radius);

		int different = CountDifferentInRect(frameA, frameB, min, max);
		bool isLit = String(probe.name).StartsWith("lit");
		if (isLit)
			EXPECT_GT(different, 10) << probe.name << " must visibly rotate between frames";
		else
			EXPECT_EQ(different, 0) << probe.name << " is static and must not change";
	}

	// Light direction is world-fixed: on both frames the bright area of the lit balls is above center
	for (auto& bitmap : { frameA, frameB })
	{
		for (auto& probe : probes)
		{
			if (String(probe.name) != "lit red" && String(probe.name) != "lit blue")
				continue;

			Vec2I center = WorldToPixel(bitmap, probe.worldPos);
			int radius = (int)(probe.radius*(float)bitmap->GetSize().x/(float)o2Render.GetResolution().x);

			long topBrightness = 0, bottomBrightness = 0;
			int count = 0;
			for (int dy = 4; dy <= radius; dy += 2)
			{
				for (int dx = -radius; dx <= radius; dx += 2)
				{
					if (dx*dx + dy*dy > radius*radius)
						continue;
					const UInt8* top = GetPixel(bitmap, center.x + dx, center.y - dy);
					const UInt8* bottom = GetPixel(bitmap, center.x + dx, center.y + dy);
					topBrightness += (int)top[0] + top[1] + top[2];
					bottomBrightness += (int)bottom[0] + bottom[1] + bottom[2];
					count++;
				}
			}
			ASSERT_GT(count, 0);
			EXPECT_GT(topBrightness, bottomBrightness)
				<< probe.name << ": the lit side must stay up while the chip spins";
		}
	}
}
