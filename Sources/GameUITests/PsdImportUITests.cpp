#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "o2/Assets/Types/ActorAsset.h"
#include "o2/Scene/CameraActor.h"
#include "o2/Scene/Components/ImageComponent.h"
#include "o2/Scene/Scene.h"
#include "o2/Utils/Bitmap/Bitmap.h"
#include "o2/Utils/Test/AppTestDriver.h"

using namespace o2;

// Validates the o2 prefab built by o2/Tools/PsdTool from testdata/ui_mock.psd:
// the hierarchy, layer order and positions must replicate the PSD layout
// (canvas 800x600; see make_test_psd.py for the reference geometry).

namespace
{
	int CountDistinctColors(const Ref<Bitmap>& bitmap)
	{
		if (!bitmap)
			return 0;

		Vector<UInt32> seen;
		const UInt32* pixels = reinterpret_cast<const UInt32*>(bitmap->GetData());
		Vec2I size = bitmap->GetSize();
		for (int y = 0; y < size.y; y += 8)
		{
			for (int x = 0; x < size.x; x += 8)
			{
				UInt32 color = pixels[y * size.x + x];
				if (!seen.Contains(color))
					seen.Add(color);
			}
		}

		return seen.Count();
	}
}

class PsdImportUI: public ::testing::Test
{
protected:
	Ref<CameraActor> camera;
	Ref<Actor>       root;

	void SetUp() override
	{
		camera = mmake<CameraActor>();
		camera->fillColor = Color4(0, 0, 0);
		camera->SetFittedSize(Vec2F(800.0f, 600.0f));
		camera->AddToScene();

		AssetRef<ActorAsset> proto("PsdImport/UiMock/ui_mock.proto");
		ASSERT_TRUE(proto);

		root = proto->Instantiate();
		root->transform->SetPosition2D(Vec2F());
		root->AddToScene();

		AppTestDriver::PumpFrames(5);
	}

	void TearDown() override
	{
		if (root)
			root->Destroy();
		if (camera)
			camera->Destroy();

		AppTestDriver::PumpFrames(2);
	}
};

TEST_F(PsdImportUI, HierarchyReplicatesPsd)
{
	// Bottom-to-top PSD order becomes child order (first child draws first)
	auto children = root->GetChildren();
	ASSERT_EQ(children.Count(), 3);
	EXPECT_EQ(children[0]->GetName(), "Background");
	EXPECT_EQ(children[1]->GetName(), "Header");
	EXPECT_EQ(children[2]->GetName(), "Panel");

	auto header = children[1];
	ASSERT_EQ(header->GetChildren().Count(), 2);
	EXPECT_EQ(header->GetChildren()[0]->GetName(), "HeaderBack");
	EXPECT_EQ(header->GetChildren()[1]->GetName(), "Title");

	auto panel = children[2];
	ASSERT_EQ(panel->GetChildren().Count(), 2);
	EXPECT_EQ(panel->GetChildren()[0]->GetName(), "PanelBack");

	auto buttons = panel->GetChildren()[1];
	EXPECT_EQ(buttons->GetName(), "Buttons");
	ASSERT_EQ(buttons->GetChildren().Count(), 2);
	EXPECT_EQ(buttons->GetChildren()[0]->GetName(), "PlayBtn");
	EXPECT_EQ(buttons->GetChildren()[1]->GetName(), "ExitBtn");

	// Groups carry no images, layers do
	EXPECT_FALSE(header->GetComponent<ImageComponent>());
	EXPECT_TRUE(header->GetChildren()[0]->GetComponent<ImageComponent>());
}

TEST_F(PsdImportUI, PositionsMatchPsdLayout)
{
	auto expectWorld = [&](const char* path, const Vec2F& expected, const Vec2F& expectedSize) {
		auto actor = root->GetChild(path);
		ASSERT_TRUE(actor) << path;
		Vec2F pos = actor->transform->GetWorldPosition2D();
		EXPECT_NEAR(pos.x, expected.x, 0.5f) << path;
		EXPECT_NEAR(pos.y, expected.y, 0.5f) << path;
		Vec2F size = actor->transform->GetSize2D();
		EXPECT_NEAR(size.x, expectedSize.x, 0.5f) << path;
		EXPECT_NEAR(size.y, expectedSize.y, 0.5f) << path;
	};

	// PSD canvas 800x600, origin remapped to the center, y up
	expectWorld("Background", Vec2F(0.0f, 0.0f), Vec2F(800.0f, 600.0f));
	expectWorld("Header", Vec2F(0.0f, 240.0f), Vec2F(800.0f, 120.0f));
	expectWorld("Header/Title", Vec2F(0.0f, 240.0f), Vec2F(200.0f, 60.0f));
	expectWorld("Panel", Vec2F(0.0f, -50.0f), Vec2F(400.0f, 300.0f));
	expectWorld("Panel/Buttons/PlayBtn", Vec2F(-80.0f, -110.0f), Vec2F(120.0f, 60.0f));
	expectWorld("Panel/Buttons/ExitBtn", Vec2F(80.0f, -110.0f), Vec2F(120.0f, 60.0f));
}

TEST_F(PsdImportUI, RendersLayeredMockup)
{
	auto bitmap = AppTestDriver::TakeScreenshot();
	ASSERT_TRUE(bitmap);

	// Background, header, title, panel and two buttons give a handful of distinct colors
	EXPECT_GE(CountDistinctColors(bitmap), 5);

	AppTestDriver::SaveScreenshot("TestScreenshots/psd_import_prefab.png");
}
