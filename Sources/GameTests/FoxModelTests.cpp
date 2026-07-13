#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "o2/Assets/Assets.h"
#include "o2/Render/SkinnedModelFormat.h"
#include "o2/Utils/FileSystem/File.h"

using namespace o2;

namespace
{
	// Loads the real Fox.glb from the project assets (CC0, see Fox.license.txt)
	bool LoadFoxModel(SkinnedModelData& model, String& error)
	{
		InFile file(o2Assets.GetAssetsPath() + "Fox.glb");
		if (!file.IsOpened())
		{
			error = "Fox.glb is not found in assets";
			return false;
		}

		Vector<UInt8> data;
		data.Resize((int)file.GetDataSize());
		file.ReadData(data.Data(), (UInt)data.Count());

		return GlbModelFormat::Parse(data.Data(), (UInt)data.Count(), model, &error);
	}
}

TEST(FoxModel, ParsesRealGlbFile)
{
	SkinnedModelData model;
	String error;
	ASSERT_TRUE(LoadFoxModel(model, error)) << error;

	EXPECT_EQ(model.positions.Count(), 1728);
	EXPECT_EQ(model.indices.Count(), 1728); // Non-indexed model: sequential triangles
	EXPECT_EQ(model.influences.Count(), 1728);
	EXPECT_EQ(model.uvs.Count(), 1728);
	EXPECT_TRUE(model.normals.IsEmpty()); // Fox has no source normals, they are computed after skinning

	EXPECT_EQ(model.nodes.Count(), 26);
	EXPECT_EQ(model.joints.Count(), 24);
	EXPECT_EQ(model.inverseBindMatrices.Count(), 24);

	ASSERT_EQ(model.animations.Count(), 3);
	EXPECT_GE(model.FindAnimation("Survey"), 0);
	EXPECT_GE(model.FindAnimation("Walk"), 0);
	EXPECT_GE(model.FindAnimation("Run"), 0);

	for (auto& animation : model.animations)
		EXPECT_GT(animation.duration, 0.1f);
}

TEST(FoxModel, BindPoseKeepsGeometry)
{
	SkinnedModelData model;
	String error;
	ASSERT_TRUE(LoadFoxModel(model, error)) << error;

	// Inverse bind matrices invert the bind pose globals: the bind palette is near identity
	// and skinning must keep the source geometry in place
	Vector<Mat4> palette;
	model.EvaluateJointsPalette(-1, 0.0f, palette);
	ASSERT_EQ(palette.Count(), 24);

	Vector<Vec3F> skinnedPositions, skinnedNormals;
	model.SkinVertices(palette, skinnedPositions, skinnedNormals);

	ASSERT_EQ(skinnedPositions.Count(), model.positions.Count());
	EXPECT_EQ(skinnedNormals.Count(), model.positions.Count());

	float maxOffset = 0.0f;
	for (int i = 0; i < model.positions.Count(); i++)
		maxOffset = Math::Max(maxOffset, (skinnedPositions[i] - model.positions[i]).Length());

	EXPECT_LT(maxOffset, 0.5f) << "bind pose skinning must keep the geometry in place";
}

TEST(FoxModel, RunAnimationMovesVertices)
{
	SkinnedModelData model;
	String error;
	ASSERT_TRUE(LoadFoxModel(model, error)) << error;

	int animation = model.FindAnimation("Run");
	ASSERT_GE(animation, 0);

	Vector<Mat4> paletteStart, paletteMiddle;
	model.EvaluateJointsPalette(animation, 0.0f, paletteStart);
	model.EvaluateJointsPalette(animation, model.animations[animation].duration*0.5f, paletteMiddle);

	Vector<Vec3F> positionsStart, positionsMiddle, normals;
	model.SkinVertices(paletteStart, positionsStart, normals);
	model.SkinVertices(paletteMiddle, positionsMiddle, normals);

	float maxOffset = 0.0f;
	for (int i = 0; i < positionsStart.Count(); i++)
		maxOffset = Math::Max(maxOffset, (positionsMiddle[i] - positionsStart[i]).Length());

	EXPECT_GT(maxOffset, 5.0f) << "Run animation must visibly move the fox vertices";
}
