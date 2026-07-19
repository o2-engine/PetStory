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

	struct ChipDef
	{
		const char* name;
		const char* albedo;
		const char* material;
		Vec2F       size;
	};

	const ChipDef kChips[] = {
		{ "red",  "ChipLit/red_df.png",      "ChipLit/red_sdf.mat",  Vec2F(210, 210) },
		{ "blue", "ChipLit/blue_df.png",     "ChipLit/blue_sdf.mat", Vec2F(210, 210) },
		{ "leaf", "ChipLit/leaf_albedo.png", "ChipLit/leaf_lit.mat", Vec2F(198, 315) },
	};

	const float kGridAngles[] = { 0, 45, 90, 135, 180, 225, 270, 315 };

	struct Cell
	{
		String name;
		float  angle;
		Vec2F  worldPos;
		Vec2F  worldSize;
	};

	Ref<Actor> MakeChip(const ChipDef& chip, const Vec2F& position, const Vec2F& size, float angleDegrees)
	{
		auto actor = mmake<Actor>(ActorCreateMode::InScene);
		actor->SetName(String(chip.name) + "_" + (String)(int)angleDegrees);
		actor->transform->SetPivot2D(Vec2F(0.5f, 0.5f));
		actor->transform->SetSize2D(size);
		actor->transform->SetPosition2D(position);
		actor->transform->SetAngleDegrees(angleDegrees);

		auto image = mmake<ImageComponent>(chip.albedo);
		image->SetMaterialAsset(AssetRef<MaterialAsset>(chip.material));
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

class ChipLitUI: public ::testing::Test
{
protected:
	Ref<CameraActor> camera;
	Vector<Cell>     cells;

	void SetUp() override
	{
		camera = mmake<CameraActor>();
		camera->SetName("chip lit camera");
		camera->fillColor = Color4(26, 26, 30);
		camera->SetFittedSize(Vec2F(1280, 1024));
		camera->AddToScene();

		// Big chips at reference scale for closeness scoring
		for (int i = 0; i < 3; i++)
		{
			Vec2F pos(-330.0f + i*330.0f, 320.0f);
			MakeChip(kChips[i], pos, kChips[i].size, 0.0f);
			cells.Add({ String(kChips[i].name) + "_big", 0.0f, pos, kChips[i].size });
		}

		// Rotation grid: every chip at every angle, scaled into 150 px columns
		for (int i = 0; i < 3; i++)
		{
			float scale = 120.0f/210.0f;
			Vec2F size = kChips[i].size*scale;
			float y = 60.0f - i*160.0f;
			for (int a = 0; a < 8; a++)
			{
				Vec2F pos(-540.0f + a*150.0f, y);
				MakeChip(kChips[i], pos, size, kGridAngles[a]);
				cells.Add({ kChips[i].name, kGridAngles[a], pos, size });
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
	Vec2F       bitmapScale; // capture bitmap size may differ from the logical resolution

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

// Materials with the chip_lit shaders compile, build and are drawn without falling
// back to the default material
TEST_F(ChipLitUI, MaterialsLoadAndBuild)
{
	for (auto& chip : kChips)
	{
		AssetRef<MaterialAsset> material(chip.material);
		ASSERT_TRUE(material) << chip.name;
		EXPECT_TRUE(material->GetVertexShader()) << chip.name;
		EXPECT_TRUE(material->GetFragmentShader()) << chip.name;
		// sdf materials add the rim LUT sampler; the lit leaf adds a normal map sampler
		EXPECT_EQ(material->GetTextureSamplers().Count(), 1) << chip.name;
	}

	auto actor = o2Scene.FindActor("red_0");
	ASSERT_TRUE(actor);
	auto image = actor->GetComponent<ImageComponent>();
	ASSERT_TRUE(image);
	ASSERT_TRUE(image->GetMaterial());
	EXPECT_TRUE(image->GetMaterial()->IsReady()) << "material must build on first draw";
}

// The lit scene renders: screenshot has the chips with per-pixel shading, and the
// screenshot + cells manifest are saved for the Python closeness scoring
TEST_F(ChipLitUI, RenderAndCaptureGrid)
{
	Capture();
	Ref<Bitmap> bitmap = screenshot;

	o2FileSystem.FolderCreate(kScreenshotsDir, true);
	EXPECT_TRUE(bitmap->Save(kScreenshotsDir + "chip_lit.png", Bitmap::ImageType::Png));

	float pixelScale = WorldToPixelScale();
	std::ofstream manifest((const char*)(kScreenshotsDir + "chip_lit_manifest.json"));
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

	// Every chip cell must contain non-background pixels
	for (auto& cell : cells)
	{
		Vec2I px = WorldToPixel(cell.worldPos);
		const UInt8* pixel = GetPixel(bitmap, px.x, px.y);
		int brightness = (int)pixel[0] + pixel[1] + pixel[2];
		EXPECT_GT(brightness, 100) << (const char*)cell.name << " at angle " << cell.angle;
	}
}

// World-fixed lighting invariant: on the round chips the upper half stays brighter
// than the lower half for every sprite rotation angle
TEST_F(ChipLitUI, LightStaysUpUnderRotation)
{
	Capture();
	Ref<Bitmap> bitmap = screenshot;

	float pixelScale = WorldToPixelScale();
	for (auto& cell : cells)
	{
		if (cell.name != "red" && cell.name != "blue")
			continue;

		Vec2I center = WorldToPixel(cell.worldPos);
		int radius = (int)(cell.worldSize.x*0.5f*pixelScale);

		long topBrightness = 0, bottomBrightness = 0;
		int count = 0;
		for (int dy = 3; dy <= radius; dy += 2)
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
			<< (const char*)cell.name << " angle " << cell.angle
			<< ": the lit half must stay up while the sprite rotates";
	}
}
