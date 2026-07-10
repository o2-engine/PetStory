#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "o2/Animation/AnimationClip.h"
#include "o2/Animation/SkinnedModelAnimation.h"
#include "o2/Assets/Assets.h"
#include "o2/Render/SkinnedModelFormat.h"
#include "o2/Utils/FileSystem/File.h"

using namespace o2;

namespace
{
	// Loads the real CesiumMan.glb from the project assets (CC-BY 4.0, see CesiumMan.license.txt)
	bool LoadCesiumManModel(SkinnedModelData& model, String& error)
	{
		InFile file(o2Assets.GetAssetsPath() + "CesiumMan.glb");
		if (!file.IsOpened())
		{
			error = "CesiumMan.glb is not found in assets";
			return false;
		}

		Vector<UInt8> data;
		data.Resize((int)file.GetDataSize());
		file.ReadData(data.Data(), (UInt)data.Count());

		return GlbModelFormat::Parse(data.Data(), (UInt)data.Count(), model, &error);
	}
}

TEST(CesiumManModel, ParsesRealGlbFile)
{
	SkinnedModelData model;
	String error;
	ASSERT_TRUE(LoadCesiumManModel(model, error)) << error;

	EXPECT_GT(model.positions.Count(), 3000);
	EXPECT_EQ(model.influences.Count(), model.positions.Count());
	EXPECT_EQ(model.normals.Count(), model.positions.Count());
	EXPECT_EQ(model.uvs.Count(), model.positions.Count());

	EXPECT_EQ(model.nodes.Count(), 22);
	EXPECT_EQ(model.joints.Count(), 19);
	EXPECT_EQ(model.inverseBindMatrices.Count(), 19);

	ASSERT_EQ(model.animations.Count(), 1);
	EXPECT_NEAR(model.animations[0].duration, 2.0f, 0.05f);
}

TEST(CesiumManModel, BindPoseKeepsGeometry)
{
	SkinnedModelData model;
	String error;
	ASSERT_TRUE(LoadCesiumManModel(model, error)) << error;

	Vector<Mat4> palette;
	model.EvaluateJointsPalette(-1, 0.0f, palette);
	ASSERT_EQ(palette.Count(), 19);

	Vector<Vec3F> skinnedPositions, skinnedNormals;
	model.SkinVertices(palette, skinnedPositions, skinnedNormals);

	// The model is authored Z-up and reoriented by the Z_UP root node: the bind pose skinning
	// keeps distances (height ~1.5 units), not raw coordinates
	AABB bounds = AABB::Bound(skinnedPositions.Data(), skinnedPositions.Count());
	EXPECT_NEAR(bounds.GetSize().Length(), 1.9f, 0.4f);
}

TEST(CesiumManModel, ClipConvertsToEngineAnimation)
{
	SkinnedModelData model;
	String error;
	ASSERT_TRUE(LoadCesiumManModel(model, error)) << error;

	auto clip = SkinnedModelAnimation::ConvertClip(model, 0);
	ASSERT_TRUE(clip);

	EXPECT_NEAR(clip->GetDuration(), 2.0f, 0.05f);
	EXPECT_GT(clip->GetTracks().Count(), 30) << "the walk cycle animates most of the 19 joints with T/R tracks";

	// Every track path targets a bone actor transform below the model root
	for (auto& track : clip->GetTracks())
	{
		EXPECT_TRUE(track->path.StartsWith("child/"));
		EXPECT_TRUE(track->path.Contains("/transform/"));
	}
}
