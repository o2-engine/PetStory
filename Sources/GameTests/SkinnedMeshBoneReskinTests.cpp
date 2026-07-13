#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "o2/Animation/SkinnedModelAnimation.h"
#include "o2/Assets/Assets.h"
#include "o2/Assets/Types/SkinnedModelAsset.h"
#include "o2/Render/SkinnedModelFormat.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/Components/SkinnedMeshComponent.h"
#include "o2/Utils/FileSystem/File.h"

using namespace o2;

namespace
{
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

	Vector<Vec3F> CopyMeshPositions(const Mesh& mesh)
	{
		Vector<Vec3F> positions;
		const Vertex* vertices = const_cast<Mesh&>(mesh).GetVertices<Vertex>();
		for (UInt i = 0; i < mesh.vertexCount; i++)
			positions.Add(Vec3F(vertices[i].x, vertices[i].y, vertices[i].z));

		return positions;
	}
}

// Moving a bone actor must reskin the mesh on the next GetMesh/Draw even without scene updates,
// like in the editor where the scene does not update while a bone is dragged
TEST(SkinnedMeshBoneReskin, BoneActorMoveUpdatesMeshWithoutSceneUpdate)
{
	SkinnedModelData model;
	String error;
	ASSERT_TRUE(LoadFoxModel(model, error)) << error;

	AssetRef<SkinnedModelAsset> asset;
	asset.CreateInstance();
	asset->SetModelData(model);

	auto actor = mmake<Actor>(ActorCreateMode::NotInScene);
	auto mesh = actor->AddComponent<SkinnedMeshComponent>();
	mesh->SetModelAsset(asset);
	mesh->SetGPUSkinningEnabled(false);
	mesh->CreateBoneActors();

	const SkinnedModelData& modelData = asset->GetModelData();

	Vector<Mat4> paletteBefore;
	mesh->EvaluateModelPalette(paletteBefore);
	ASSERT_FALSE(paletteBefore.IsEmpty());

	Vector<Vec3F> positionsBefore = CopyMeshPositions(mesh->GetMesh());
	ASSERT_FALSE(positionsBefore.IsEmpty());

	// Root joint moves the whole skeleton, every skinned vertex must follow
	auto rootBone = SkinnedModelAnimation::FindBoneActor(actor, modelData, modelData.joints[0]);
	ASSERT_NE(rootBone, nullptr);
	rootBone->transform->SetPosition(rootBone->transform->GetPosition() + Vec3F(0.0f, 20.0f, 0.0f));

	Vector<Mat4> paletteAfter;
	mesh->EvaluateModelPalette(paletteAfter);
	EXPECT_TRUE(paletteAfter != paletteBefore) << "palette must follow the moved bone";

	Vector<Vec3F> positionsAfter = CopyMeshPositions(mesh->GetMesh());
	ASSERT_EQ(positionsAfter.Count(), positionsBefore.Count());

	float maxOffset = 0.0f;
	for (int i = 0; i < positionsBefore.Count(); i++)
		maxOffset = Math::Max(maxOffset, (positionsAfter[i] - positionsBefore[i]).Length());

	EXPECT_GT(maxOffset, 5.0f) << "mesh must be reskinned after the bone actor moved";
}
