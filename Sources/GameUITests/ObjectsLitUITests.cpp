#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include <fstream>

#include "o2/Assets/Types/MaterialAsset.h"
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

	struct ObjectDef
	{
		const char* name;
		Vec2F       size;
	};

	// sizes = source reference sizes (px)
	const ObjectDef kObjects[] = {
		{ "acorn",   Vec2F(249, 307) },
		{ "bandage", Vec2F(245, 298) },
		{ "bone",    Vec2F(234, 236) },
		{ "brush",   Vec2F(239, 278) },
		{ "food",    Vec2F(374, 347) },
		{ "leaf",    Vec2F(198, 315) },
		{ "patch",   Vec2F(212, 268) },
		{ "pill",    Vec2F(233, 220) },
		{ "pillow",  Vec2F(261, 242) },
		{ "soap",    Vec2F(275, 247) },
		{ "water",   Vec2F(208, 275) },
		{ "wood",    Vec2F(258, 276) },
	};

	const float kGridAngles[] = { 0, 45, 90, 135, 180, 225, 270, 315 };

	struct Cell
	{
		String name;
		float  angle;
		Vec2F  worldPos;
		Vec2F  worldSize;
	};

	Ref<Actor> MakeObject(const ObjectDef& def, const Vec2F& position, const Vec2F& size, float angleDegrees)
	{
		auto actor = mmake<Actor>(ActorCreateMode::InScene);
		actor->SetName(String(def.name) + "_" + (String)(int)angleDegrees);
		actor->transform->SetPivot2D(Vec2F(0.5f, 0.5f));
		actor->transform->SetSize2D(size);
		actor->transform->SetPosition2D(position);
		actor->transform->SetAngleDegrees(angleDegrees);

		auto image = mmake<ImageComponent>(String("Game field/Objects/") + def.name + "_alb.png");
		image->SetMaterialAsset(AssetRef<MaterialAsset>(String("Game field/Objects/") + def.name + "_obj.mat"));
		actor->AddComponent(image);

		return actor;
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

class ObjectsLitUI: public ::testing::Test
{
protected:
	Ref<CameraActor> camera;
	Vector<Cell>     cells;

	void SetUp() override
	{
		camera = mmake<CameraActor>();
		camera->SetName("objects lit camera");
		camera->fillColor = Color4(26, 26, 30);
		camera->SetFittedSize(Vec2F(1600, 1200));
		camera->AddToScene();

		// All objects at reference scale (x0.7): two rows of six
		for (int i = 0; i < 12; i++)
		{
			Vec2F size = kObjects[i].size*0.7f;
			Vec2F pos(-625.0f + (i % 6)*250.0f, i < 6 ? 430.0f : 160.0f);
			MakeObject(kObjects[i], pos, size, 0.0f);
			cells.Add({ String(kObjects[i].name) + "_big", 0.0f, pos, size });
		}

		// Rotation grid for shape-diverse representatives: every angle in 180 px columns
		const char* reps[] = { "patch", "brush", "bandage" };
		for (int r = 0; r < 3; r++)
		{
			int idx = 0;
			for (int i = 0; i < 12; i++)
				if (String(kObjects[i].name) == reps[r]) idx = i;
			Vec2F size = kObjects[idx].size*(140.0f/300.0f);
			float y = -120.0f - r*190.0f;
			for (int a = 0; a < 8; a++)
			{
				Vec2F pos(-630.0f + a*180.0f, y);
				MakeObject(kObjects[idx], pos, size, kGridAngles[a]);
				cells.Add({ kObjects[idx].name, kGridAngles[a], pos, size });
			}
		}

		AppTestDriver::PumpFrames(5);
	}

	void TearDown() override
	{
		camera = nullptr;
		o2Scene.Clear(true);
		o2Scene.UpdateDestroyingEntities();
		AppTestDriver::PumpFrames(2);
	}

	Ref<Bitmap> screenshot;
	Vec2F       bitmapScale;

	void Capture()
	{
		screenshot = AppTestDriver::TakeScreenshot();
		ASSERT_TRUE(screenshot);
		Vec2F resolution = (Vec2F)o2Render.GetResolution();
		Vec2F bitmapSize = (Vec2F)screenshot->GetSize();
		bitmapScale = Vec2F(bitmapSize.x/resolution.x, bitmapSize.y/resolution.y);
	}

	Vec2I WorldToPixel(const Vec2F& worldPos)
	{
		Vec2F screen = camera->listenersLayer->ScreenFromLocal(worldPos);
		Vec2F center = (Vec2F)o2Render.GetResolution()*0.5f;
		return Vec2I((int)((screen.x + center.x)*bitmapScale.x), (int)((center.y - screen.y)*bitmapScale.y));
	}

	float WorldToPixelScale()
	{
		Vec2F origin = camera->listenersLayer->ScreenFromLocal(Vec2F(0, 0));
		Vec2F unit = camera->listenersLayer->ScreenFromLocal(Vec2F(100, 0));
		return (unit - origin).Length()/100.0f*bitmapScale.x;
	}
};

// Materials with the object_lit shader compile, build and expose the normal-map sampler
TEST_F(ObjectsLitUI, MaterialsLoadAndBuild)
{
	for (auto& def : kObjects)
	{
		AssetRef<MaterialAsset> material(String("Game field/Objects/") + def.name + "_obj.mat");
		ASSERT_TRUE(material) << def.name;
		EXPECT_TRUE(material->GetVertexShader()) << def.name;
		EXPECT_TRUE(material->GetFragmentShader()) << def.name;
		EXPECT_EQ(material->GetTextureSamplers().Count(), 1) << def.name;
	}

	auto actor = o2Scene.FindActor("acorn_0");
	ASSERT_TRUE(actor);
	auto image = actor->GetComponent<ImageComponent>();
	ASSERT_TRUE(image);
	ASSERT_TRUE(image->GetMaterial());
	EXPECT_TRUE(image->GetMaterial()->IsReady()) << "material must build on first draw";
}

// The grid renders and the screenshot + manifest are saved for the Python scoring
TEST_F(ObjectsLitUI, RenderAndCaptureGrid)
{
	Capture();
	Ref<Bitmap> bitmap = screenshot;

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	EXPECT_TRUE(bitmap->Save(kScreenshotsDir + "objects_lit.png", Bitmap::ImageType::Png));

	float pixelScale = WorldToPixelScale();
	std::ofstream manifest((const char*)(kScreenshotsDir + "objects_lit_manifest.json"));
	ASSERT_TRUE(manifest.is_open());
	manifest << "[\n";
	for (int i = 0; i < cells.Count(); i++)
	{
		auto& cell = cells[i];
		Vec2I px = WorldToPixel(cell.worldPos);
		manifest << "  {\"name\": \"" << (const char*)cell.name << "\", \"angle\": " << cell.angle
			<< ", \"cx\": " << px.x << ", \"cy\": " << px.y
			<< ", \"w\": " << (int)(cell.worldSize.x*pixelScale)
			<< ", \"h\": " << (int)(cell.worldSize.y*pixelScale) << "}"
			<< (i + 1 < cells.Count() ? "," : "") << "\n";
	}
	manifest << "]\n";
	manifest.close();

	for (auto& cell : cells)
	{
		Vec2I px = WorldToPixel(cell.worldPos);
		const UInt8* pixel = GetPixel(bitmap, px.x, px.y);
		int brightness = (int)pixel[0] + pixel[1] + pixel[2];
		EXPECT_GT(brightness, 60) << (const char*)cell.name << " at angle " << cell.angle;
	}
}
