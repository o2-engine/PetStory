#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include <fstream>

#include "o2/Assets/Assets.h"
#include "o2/Render/Render.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Scene/Scene.h"
#include "o2/Utils/Bitmap/Bitmap.h"
#include "o2/Utils/FileSystem/FileSystem.h"
#include "o2/Utils/Test/AppTestDriver.h"

using namespace o2;

namespace
{
	const String kScreenshotsDir = "TestScreenshots/";
	const char* kChipNames[] = { "ChipRed", "ChipBlue", "ChipGreen", "ChipOrange", "ChipViolet", "ChipYellow" };

	// y is in top-down image coordinates; the bitmap stores rows bottom-up
	const UInt8* GetPixel(const Ref<Bitmap>& bitmap, int x, int y)
	{
		Vec2I size = bitmap->GetSize();
		x = Math::Clamp(x, 0, size.x - 1);
		y = Math::Clamp(y, 0, size.y - 1);
		return bitmap->GetData() + ((size.y - 1 - y)*size.x + x)*4;
	}
}

class TestSceneUI: public ::testing::Test
{
protected:
	Ref<CameraActor> camera;

	void SetUp() override
	{
		o2Scene.Load(o2Assets.GetBuiltAssetsPath() + String("test.scn"));
		AppTestDriver::PumpFrames(5);

		for (auto& weakCamera : o2Scene.GetCameras())
		{
			if (auto sceneCamera = weakCamera.Lock())
				camera = sceneCamera;
		}
	}

	void TearDown() override
	{
		camera = nullptr;
		o2Scene.Clear(true);
		o2Scene.UpdateDestroyingEntities();
		AppTestDriver::PumpFrames(2);
	}
};

// test.scn instantiates every chip color prototype with the sdf material; the scene
// renders and each chip appears on screen at its position
TEST_F(TestSceneUI, AllChipPrototypesRenderWithSdfMaterials)
{
	ASSERT_TRUE(camera);

	for (auto name : kChipNames)
	{
		auto actor = o2Scene.FindActor(name);
		ASSERT_TRUE(actor) << name;

		auto image = actor->GetComponent<ImageComponent>();
		ASSERT_TRUE(image) << name;
		ASSERT_TRUE(image->GetMaterialAsset()) << name << " must use a material asset";
		EXPECT_TRUE(image->GetMaterialAsset()->GetPath().EndsWith("_sdf.mat")) << name;
		ASSERT_TRUE(image->GetMaterial()) << name;
		EXPECT_TRUE(image->GetMaterial()->IsReady()) << name << ": sdf material must build";
	}

	Ref<Bitmap> bitmap = AppTestDriver::TakeScreenshot();
	ASSERT_TRUE(bitmap);
	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	EXPECT_TRUE(bitmap->Save(kScreenshotsDir + "test_scn.png", Bitmap::ImageType::Png));

	Vec2F resolution = (Vec2F)o2Render.GetResolution();
	Vec2F bitmapSize = (Vec2F)bitmap->GetSize();

	std::ofstream manifest((const char*)(kScreenshotsDir + "test_scn_manifest.json"));
	ASSERT_TRUE(manifest.is_open());
	manifest << "[\n";
	for (int i = 0; i < 6; i++)
	{
		auto actor = o2Scene.FindActor(kChipNames[i]);
		Vec2F world = actor->transform->GetWorldPosition().XY();
		Vec2F screen = camera->listenersLayer->ScreenFromLocal(world);
		Vec2F center = resolution*0.5f;
		int px = (int)((screen.x + center.x)*bitmapSize.x/resolution.x);
		int py = (int)((center.y - screen.y)*bitmapSize.y/resolution.y);

		Vec2F origin = camera->listenersLayer->ScreenFromLocal(Vec2F(0, 0));
		Vec2F unit = camera->listenersLayer->ScreenFromLocal(Vec2F(100, 0));
		float pixelScale = (unit - origin).Length()/100.0f*bitmapSize.x/resolution.x;
		int sizePx = (int)(actor->transform->GetSize().x*actor->transform->GetScale().x*pixelScale);

		manifest << "  {\"name\": \"" << kChipNames[i] << "\", \"cx\": " << px << ", \"cy\": " << py
			<< ", \"size\": " << sizePx << "}" << (i < 5 ? "," : "") << "\n";

		// the chip must be visible: its center pixel is not the background
		const UInt8* pixel = GetPixel(bitmap, px, py);
		int brightness = (int)pixel[0] + pixel[1] + pixel[2];
		EXPECT_GT(brightness, 100) << kChipNames[i] << " at " << px << ", " << py;
	}
	manifest << "]\n";
}

// Object chips: the spawner releases them one at a time at the top; anything
// reaching the bottom sensor trigger is removed and later respawned from the top
TEST_F(TestSceneUI, ObjectChipsSpawnAndBottomTriggerRemoves)
{
	auto container = o2Scene.FindActor("FallingObjects");
	auto trigger = o2Scene.FindActor("ObjBottomTrigger");
	ASSERT_TRUE(container);
	ASSERT_TRUE(trigger);

	// the spawner releases objects one at a time up to its limit
	AppTestDriver::PumpFrames(60);
	int count = container->GetChildren().Count();
	ASSERT_GE(count, 1) << "the spawner must release at least one object";
	EXPECT_LE(count, 3);

	// physics moves the object: it must be a rigid body with colliders and fall freely
	auto object = container->GetChildren()[0];
	float y0 = object->transform->GetPosition().y;
	AppTestDriver::PumpFrames(60);
	float y1 = object->transform->GetPosition().y;
	EXPECT_LT(y1, y0) << "the object chip must fall under physics";

	// teleport the object into the bottom trigger: it must be removed
	String objectName = object->GetName();
	object->transform->position2D = trigger->transform->worldPosition2D.Get();
	AppTestDriver::PumpFrames(10);
	bool stillThere = container->GetChildren().Contains([&](auto& a) { return a == object; });
	EXPECT_FALSE(stillThere) << "the bottom trigger must remove the object chip";
}
