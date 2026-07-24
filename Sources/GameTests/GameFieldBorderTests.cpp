#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "Box2D/Collision/Shapes/b2ChainShape.h"
#include "Level/GameFieldBorder.h"
#include "o2/Physics/PhysicsWorld.h"
#include "o2/Scene/Actor.h"
#include "o2/Scene/Physics/CircleCollider.h"
#include "o2/Scene/Physics/RigidBody.h"
#include "o2/Scene/Scene.h"
#include "Scene/SceneTestHelpers.h"

using namespace o2;

namespace
{
    struct GameFieldBorderProbe: GameFieldBorder
    {
        b2Shape* Shape(const Basis& basis) { return GetShape(basis); }
    };

    void StepPhysics(int frames)
    {
        for (int i = 0; i < frames; i++)
        {
            o2Physics.PreUpdate();
            o2Physics.Update(1.0f/60.0f);
            o2Physics.PostUpdate();
        }
    }
}

TEST(GameFieldBorderTests, DefaultIsClosedRectangle)
{
    auto border = mmake<GameFieldBorder>();

    EXPECT_TRUE(border->IsLoop());
    EXPECT_TRUE(border->spline->IsClosed());
    EXPECT_EQ(border->spline->GetKeys().Count(), 4);

    auto polygon = border->GetLocalPolygon();
    ASSERT_GE(polygon.Count(), 5);
    EXPECT_LT((polygon[0] - polygon.Last()).Length(), 0.1f);

    RectF bounds(polygon[0], polygon[0]);
    for (auto& p : polygon)
    {
        bounds.left = Math::Min(bounds.left, p.x);
        bounds.right = Math::Max(bounds.right, p.x);
        bounds.bottom = Math::Min(bounds.bottom, p.y);
        bounds.top = Math::Max(bounds.top, p.y);
    }

    EXPECT_NEAR(bounds.Width(), 600.0f, 1.0f);
    EXPECT_NEAR(bounds.Height(), 400.0f, 1.0f);
}

TEST(GameFieldBorderTests, SerializationRoundtrip)
{
    auto border = mmake<GameFieldBorder>();
    border->SetBackTileSize(333.0f);
    border->SetInnerShadowWidth(44.0f);
    border->SetDropShadowWidth(55.0f);
    border->SetDropShadowOffset(Vec2F(3.0f, -7.0f));
    border->SetWidth(48.0f);

    auto key = border->spline->GetKey(0);
    key.value = Vec2F(-250.0f, -180.0f);
    border->spline->SetKey(key, 0);

    DataDocument data;
    border->Serialize(data);

    auto restored = mmake<GameFieldBorder>();
    restored->Deserialize(data);

    EXPECT_FLOAT_EQ(restored->GetBackTileSize(), 333.0f);
    EXPECT_FLOAT_EQ(restored->GetInnerShadowWidth(), 44.0f);
    EXPECT_FLOAT_EQ(restored->GetDropShadowWidth(), 55.0f);
    EXPECT_NEAR(restored->GetDropShadowOffset().x, 3.0f, 0.001f);
    EXPECT_NEAR(restored->GetDropShadowOffset().y, -7.0f, 0.001f);
    EXPECT_FLOAT_EQ(restored->GetWidth(), 48.0f);
    EXPECT_TRUE(restored->IsLoop());
    ASSERT_EQ(restored->spline->GetKeys().Count(), 4);
    EXPECT_NEAR(restored->spline->GetKey(0).value.x, -250.0f, 0.001f);
    EXPECT_NEAR(restored->spline->GetKey(0).value.y, -180.0f, 0.001f);
}

TEST(GameFieldBorderTests, RebuildsFieldMeshesHeadless)
{
    SceneCleanGuard guard;

    auto field = mmake<RigidBody>();
    field->SetBodyType(RigidBody::Type::Static);
    auto border = mmake<GameFieldBorder>();
    field->AddComponent(border);
    TickFrame();

    border->RebuildFieldMeshes();

    auto polygon = border->GetLocalPolygon();
    ASSERT_GE(polygon.Count(), 5);

    EXPECT_GT((int)border->GetBackMesh().polyCount, 0);
    EXPECT_GT((int)border->GetInnerShadowMesh().polyCount, 0);
    EXPECT_GT((int)border->GetDropShadowMesh().polyCount, 0);
    EXPECT_EQ((int)border->GetInnerShadowMesh().vertexCount, polygon.Count()*2);
    EXPECT_EQ((int)border->GetDropShadowMesh().vertexCount, polygon.Count()*2);
}

// The closed chain must include the closing bezier segment (last key -> first key),
// not replace it with a straight chord
TEST(GameFieldBorderTests, LoopChainIncludesClosingCurve)
{
    auto sharp = mmake<GameFieldBorderProbe>();
    auto sharpShape = dynamic_cast<b2ChainShape*>(sharp->Shape(Basis::Identity()));
    ASSERT_NE(sharpShape, nullptr);

    // Four rect segments at ~20 approximation points each; before the fix the
    // closing segment contributed nothing (~58 verts)
    EXPECT_GE(sharpShape->m_count, 70);

    // Curve the closing segment (key3 (-300,200) -> key0 (-300,-200)) leftwards:
    // the chain must leave the rect bounds (all base verts have x >= -300)
    auto rounded = mmake<GameFieldBorderProbe>();
    auto key = rounded->spline->GetKey(0);
    key.prevSupport = Vec2F(-60.0f, 0.0f);
    rounded->spline->SetKey(key, 0);

    auto roundedShape = dynamic_cast<b2ChainShape*>(rounded->Shape(Basis::Identity()));
    ASSERT_NE(roundedShape, nullptr);

    float minX = 0.0f;
    for (int i = 0; i < roundedShape->m_count; i++)
        minX = Math::Min(minX, roundedShape->m_vertices[i].x);

    EXPECT_LT(minX, -305.0f) << "closing segment must follow the bezier curve";
}

TEST(GameFieldBorderTests, PhysicsHoldsBallInsideField)
{
    if (!PhysicsWorld::IsSingletonInitialzed())
        GTEST_SKIP() << "physics world is not initialized";

    SceneCleanGuard guard;

    auto field = mmake<RigidBody>();
    field->SetBodyType(RigidBody::Type::Static);
    field->AddComponent(mmake<GameFieldBorder>());

    auto ball = mmake<RigidBody>();
    ball->transform->SetPosition2D(Vec2F(30.0f, 100.0f));
    ball->SetIsBullet(true);
    auto ballCollider = mmake<CircleCollider>();
    ballCollider->SetRadius(20.0f);
    ball->AddComponent(ballCollider);

    TickFrame();
    StepPhysics(600);

    Vec2F pos = ball->transform->GetWorldPosition2D();
    EXPECT_GT(pos.y, -260.0f) << "ball must rest on the bottom border";
    EXPECT_LT(pos.y, 0.0f) << "ball must fall from its spawn height";
    EXPECT_LT(Math::Abs(pos.x), 340.0f);
}

TEST(GameFieldBorderTests, BallFallsThroughWithoutBorder)
{
    if (!PhysicsWorld::IsSingletonInitialzed())
        GTEST_SKIP() << "physics world is not initialized";

    SceneCleanGuard guard;

    auto ball = mmake<RigidBody>();
    ball->transform->SetPosition2D(Vec2F(30.0f, 100.0f));
    auto ballCollider = mmake<CircleCollider>();
    ballCollider->SetRadius(20.0f);
    ball->AddComponent(ballCollider);

    TickFrame();
    StepPhysics(600);

    EXPECT_LT(ball->transform->GetWorldPosition2D().y, -400.0f);
}
