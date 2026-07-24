#include "o2/stdafx.h"
#include <gtest/gtest.h>

#include "Level/ChipsSpawner.h"

using namespace o2;

TEST(ChipsSpawnerTests, FindsPositionInEmptyZone)
{
    RectF zone(-500.0f, -50.0f, 500.0f, 50.0f);

    Vec2F result;
    bool found = ChipsSpawnerComponent::FindFreeSpawnPosition(zone, {}, 220.0f, 8, result);

    ASSERT_TRUE(found);
    EXPECT_GE(result.x, zone.left);
    EXPECT_LE(result.x, zone.right);
    EXPECT_GE(result.y, zone.bottom);
    EXPECT_LE(result.y, zone.top);
}

TEST(ChipsSpawnerTests, RespectsClearanceToOccupiedPoints)
{
    RectF zone(-500.0f, -50.0f, 500.0f, 50.0f);
    Vector<Vec2F> occupied = { Vec2F(-250.0f, 0.0f), Vec2F(250.0f, 0.0f) };

    for (int i = 0; i < 50; i++)
    {
        Vec2F result;
        if (!ChipsSpawnerComponent::FindFreeSpawnPosition(zone, occupied, 220.0f, 8, result))
            continue;

        for (auto& point : occupied)
            EXPECT_GE((point - result).Length(), 220.0f);
    }
}

TEST(ChipsSpawnerTests, CrowdedZoneSkipsSpawn)
{
    // Single occupied point whose clearance covers the whole zone
    RectF zone(-50.0f, -50.0f, 50.0f, 50.0f);
    Vector<Vec2F> occupied = { Vec2F(0.0f, 0.0f) };

    Vec2F result;
    bool found = ChipsSpawnerComponent::FindFreeSpawnPosition(zone, occupied, 300.0f, 16, result);

    EXPECT_FALSE(found);
}
