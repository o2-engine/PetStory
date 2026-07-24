#include "o2/stdafx.h"
#include "Level/GameFieldBorder.h"

#include "3rdPartyLibs/CDT/include/CDT.h"
#include "o2/Assets/Assets.h"
#include "o2/Render/Render.h"
#include "o2/Scene/Actor.h"
#include "o2/Utils/Bitmap/Bitmap.h"

namespace
{
    const char* kDefaultBorderImage = "Game field/FieldBorderTile.png";
    const char* kDefaultBackImage = "Game field/FieldBack.png";
    const char* kDefaultShadowImage = "Game field/FieldShadowTile.png";

    // Bitmaps load bottom-up: PNG top row (opaque shadow side) maps to v = 1
    const float kOpaqueV = 1.0f;
    const float kTransparentV = 0.0f;
}

GameFieldBorder::GameFieldBorder()
{
    mIsLoop = true;
    mWidth = 36.0f;

    spline->BeginKeysBatchChange();
    spline->RemoveAllKeys();
    spline->AppendKey(Vec2F(-300.0f, -200.0f), 0.0f, Vec2F(), Vec2F());
    spline->AppendKey(Vec2F(300.0f, -200.0f), 0.0f, Vec2F(), Vec2F());
    spline->AppendKey(Vec2F(300.0f, 200.0f), 0.0f, Vec2F(), Vec2F());
    spline->AppendKey(Vec2F(-300.0f, 200.0f), 0.0f, Vec2F(), Vec2F());
    spline->CompleteKeysBatchingChange();
    spline->SetClosed(true);
}

GameFieldBorder::GameFieldBorder(const GameFieldBorder& other):
    SplineMeshCollider(other),
    mBackImage(other.mBackImage), mBackTileSize(other.mBackTileSize), mShadowImage(other.mShadowImage),
    mInnerShadowWidth(other.mInnerShadowWidth), mDropShadowWidth(other.mDropShadowWidth),
    mDropShadowOffset(other.mDropShadowOffset)
{
    mNeedUpdateFieldMeshes = true;
}

GameFieldBorder::~GameFieldBorder() = default;

GameFieldBorder& GameFieldBorder::operator=(const GameFieldBorder& other)
{
    SplineMeshCollider::operator=(other);
    mBackImage = other.mBackImage;
    mBackTileSize = other.mBackTileSize;
    mShadowImage = other.mShadowImage;
    mInnerShadowWidth = other.mInnerShadowWidth;
    mDropShadowWidth = other.mDropShadowWidth;
    mDropShadowOffset = other.mDropShadowOffset;
    mBackTexture = TextureRef();
    mShadowTexture = TextureRef();
    mNeedUpdateFieldMeshes = true;
    return *this;
}

void GameFieldBorder::SetBackImage(const AssetRef<ImageAsset>& image)
{
    mBackImage = image;
    mBackTexture = TextureRef();
    mNeedUpdateFieldMeshes = true;
}

const AssetRef<ImageAsset>& GameFieldBorder::GetBackImage() const
{
    return mBackImage;
}

void GameFieldBorder::SetBackTileSize(float size)
{
    mBackTileSize = size;
    mNeedUpdateFieldMeshes = true;
}

float GameFieldBorder::GetBackTileSize() const
{
    return mBackTileSize;
}

void GameFieldBorder::SetShadowImage(const AssetRef<ImageAsset>& image)
{
    mShadowImage = image;
    mShadowTexture = TextureRef();
    mNeedUpdateFieldMeshes = true;
}

const AssetRef<ImageAsset>& GameFieldBorder::GetShadowImage() const
{
    return mShadowImage;
}

void GameFieldBorder::SetInnerShadowWidth(float width)
{
    mInnerShadowWidth = width;
    mNeedUpdateFieldMeshes = true;
}

float GameFieldBorder::GetInnerShadowWidth() const
{
    return mInnerShadowWidth;
}

void GameFieldBorder::SetDropShadowWidth(float width)
{
    mDropShadowWidth = width;
    mNeedUpdateFieldMeshes = true;
}

float GameFieldBorder::GetDropShadowWidth() const
{
    return mDropShadowWidth;
}

void GameFieldBorder::SetDropShadowOffset(const Vec2F& offset)
{
    mDropShadowOffset = offset;
    mNeedUpdateFieldMeshes = true;
}

const Vec2F& GameFieldBorder::GetDropShadowOffset() const
{
    return mDropShadowOffset;
}

String GameFieldBorder::GetName()
{
    return "Game field border";
}

String GameFieldBorder::GetCategory()
{
    return "Game";
}

bool GameFieldBorder::IsAvailableFromCreateMenu()
{
    return true;
}

void GameFieldBorder::OnSplineChanged()
{
    SplineMeshCollider::OnSplineChanged();
    mNeedUpdateFieldMeshes = true;
}

void GameFieldBorder::OnTransformUpdated()
{
    SplineMeshCollider::OnTransformUpdated();
    mNeedUpdateFieldMeshes = true;
}

void GameFieldBorder::OnAddToScene()
{
    SplineMeshCollider::OnAddToScene();
    mNeedUpdateFieldMeshes = true;
}

void GameFieldBorder::OnDraw()
{
    if (mNeedUpdateFieldMeshes)
        UpdateFieldMeshes();

    if (mDropShadowMesh.polyCount > 0)
        mDropShadowMesh.Draw();

    if (mBackMesh.polyCount > 0)
        mBackMesh.Draw();

    if (mInnerShadowMesh.polyCount > 0)
        mInnerShadowMesh.Draw();

    if (mBorderMesh.polyCount > 0)
        mBorderMesh.Draw();
}

Vector<Vec2F> GameFieldBorder::GetLocalPolygon() const
{
    Vector<Vec2F> path;

    if (!spline)
        return path;

    const auto& keys = spline->GetKeys();
    if (keys.Count() < 3)
        return path;

    auto appendKeySegment = [&](int keyIndex, bool skipFirst) {
        const ApproximationVec2F* approx = keys[keyIndex].GetApproximatedPointsLeft();
        int count = keys[keyIndex].GetApproximatedPointsCount();
        for (int j = skipFirst ? 1 : 0; j < count; j++)
        {
            Vec2F p = approx[j].value;
            if (!path.IsEmpty() && (p - path.Last()).Length() < 0.001f)
                continue;

            path.Add(p);
        }
    };

    for (int i = 1; i < keys.Count(); i++)
        appendKeySegment(i, i > 1);

    if (mIsLoop)
    {
        appendKeySegment(0, true);

        if (!path.IsEmpty() && (path.Last() - path[0]).Length() > 0.001f)
            path.Add(path[0]);
    }

    return path;
}

void GameFieldBorder::RebuildFieldMeshes()
{
    UpdateFieldMeshes();
}

const Mesh& GameFieldBorder::GetBackMesh() const
{
    return mBackMesh;
}

const Mesh& GameFieldBorder::GetInnerShadowMesh() const
{
    return mInnerShadowMesh;
}

const Mesh& GameFieldBorder::GetDropShadowMesh() const
{
    return mDropShadowMesh;
}

const Mesh& GameFieldBorder::GetBorderMesh() const
{
    return mBorderMesh;
}

void GameFieldBorder::EnsureDefaultAssets()
{
    // Atlas image loading touches the render device, so defaults are drawing-only
    if (mDefaultAssetsChecked || !Assets::IsSingletonInitialzed() || !Render::IsSingletonInitialzed())
        return;

    mDefaultAssetsChecked = true;

    if (!mImage)
        SetImage(AssetRef<ImageAsset>(String(kDefaultBorderImage)));

    if (!mBackImage)
        SetBackImage(AssetRef<ImageAsset>(String(kDefaultBackImage)));

    if (!mShadowImage)
        SetShadowImage(AssetRef<ImageAsset>(String(kDefaultShadowImage)));
}

TextureRef GameFieldBorder::CreateRepeatTexture(const AssetRef<ImageAsset>& image) const
{
    if (!image || !Render::IsSingletonInitialzed())
        return TextureRef();

    auto bitmap = const_cast<ImageAsset*>(image.Get())->GetBitmap();
    if (!bitmap)
        return TextureRef();

    TextureRef texture(*bitmap);
    texture->SetWrap(Texture::Wrap::Repeat);
    return texture;
}

void GameFieldBorder::UpdateFieldMeshes()
{
    mNeedUpdateFieldMeshes = false;

    EnsureDefaultAssets();

    mBackMesh.vertexCount = 0;
    mBackMesh.polyCount = 0;
    mInnerShadowMesh.vertexCount = 0;
    mInnerShadowMesh.polyCount = 0;
    mDropShadowMesh.vertexCount = 0;
    mDropShadowMesh.polyCount = 0;
    mBorderMesh.vertexCount = 0;
    mBorderMesh.polyCount = 0;

    auto owner = mOwner.Lock();
    if (!mIsLoop || !owner)
        return;

    Vector<Vec2F> path = GetLocalPolygon();
    if (path.Count() < 4)
        return;

    if (!mBackTexture)
        mBackTexture = CreateRepeatTexture(mBackImage);

    if (!mShadowTexture)
        mShadowTexture = CreateRepeatTexture(mShadowImage);

    if (!mBorderTexture || mBorderTextureImage != mImage)
    {
        mBorderTexture = CreateRepeatTexture(mImage);
        mBorderTextureImage = mImage;
    }

    Basis transform = owner->transform->GetWorldNonSizedBasis();

    // Positive signed area = CCW path, then the left perpendicular (-t.y, t.x) points inward
    float doubleArea = 0.0f;
    for (int i = 0; i < path.Count() - 1; i++)
        doubleArea += path[i].x * path[i + 1].y - path[i + 1].x * path[i].y;

    float inwardSign = doubleArea > 0.0f ? 1.0f : -1.0f;

    BuildBackMesh(path, transform);

    float shadowAspect = 0.25f;
    if (mShadowImage)
    {
        Vec2F size = mShadowImage->GetSize();
        if (size.y > 0.0f)
            shadowAspect = size.x / size.y;
    }

    float borderAspect = 1.0f;
    if (mImage)
    {
        Vec2F size = mImage->GetSize();
        if (size.y > 0.0f)
            borderAspect = size.x / size.y;
    }

    BuildStripMesh(mInnerShadowMesh, path, transform, inwardSign,
                   0.0f, mInnerShadowWidth, Vec2F(),
                   mShadowTexture, Math::Max(mInnerShadowWidth * shadowAspect, 1.0f),
                   kOpaqueV, kTransparentV, Color4::White());

    BuildStripMesh(mDropShadowMesh, path, transform, inwardSign,
                   mWidth * 0.5f, -mDropShadowWidth, mDropShadowOffset,
                   mShadowTexture, Math::Max(mDropShadowWidth * shadowAspect, 1.0f),
                   kOpaqueV, kTransparentV, Color4::White());

    BuildStripMesh(mBorderMesh, path, transform, inwardSign,
                   -mWidth * 0.5f, mWidth * 0.5f, Vec2F(),
                   mBorderTexture, Math::Max(mWidth * borderAspect, 1.0f),
                   1.0f, 0.0f, mColor);
}

void GameFieldBorder::BuildBackMesh(const Vector<Vec2F>& path, const Basis& transform)
{
    std::vector<CDT::V2d<float>> vertices;
    std::vector<CDT::Edge> edges;

    // Path is closed (last point == first) — drop the duplicate and close by edge
    int count = path.Count() - 1;
    if (count < 3)
        return;

    for (int i = 0; i < count; i++)
    {
        vertices.push_back(CDT::V2d<float>::make(path[i].x, path[i].y));
        if (i > 0)
            edges.push_back(CDT::Edge(i - 1, i));
    }
    edges.push_back(CDT::Edge(count - 1, 0));

    CDT::Triangulation<float> triangulation(CDT::VertexInsertionOrder::AsProvided);
    triangulation.insertVertices(vertices);
    triangulation.insertEdges(edges);
    triangulation.eraseOuterTriangles();

    if (triangulation.triangles.empty())
        return;

    mBackMesh.Resize((UInt)triangulation.vertices.size(), (UInt)triangulation.triangles.size());

    float invTile = 1.0f / Math::Max(mBackTileSize, 1.0f);
    UInt32 color = Color4::White().ARGB();

    Vertex* verts = mBackMesh.GetVertices<Vertex>();
    for (size_t i = 0; i < triangulation.vertices.size(); i++)
    {
        Vec2F p(triangulation.vertices[i].x, triangulation.vertices[i].y);
        verts[i].Set(p * transform, 1.0f, color, p.x * invTile, p.y * invTile);
    }

    VertexIndex* idx = mBackMesh.GetIndexes();
    for (size_t i = 0; i < triangulation.triangles.size(); i++)
    {
        idx[i * 3 + 0] = triangulation.triangles[i].vertices[0];
        idx[i * 3 + 1] = triangulation.triangles[i].vertices[1];
        idx[i * 3 + 2] = triangulation.triangles[i].vertices[2];
    }

    mBackMesh.SetTexture(mBackTexture);
    mBackMesh.vertexCount = (UInt)triangulation.vertices.size();
    mBackMesh.polyCount = (UInt)triangulation.triangles.size();
}

void GameFieldBorder::BuildStripMesh(Mesh& mesh, const Vector<Vec2F>& path, const Basis& transform, float inwardSign,
                                     float offsetA, float offsetB, const Vec2F& shift, const TextureRef& texture,
                                     float tileLength, float vA, float vB, const Color4& colorValue)
{
    int n = path.Count();
    if (n < 3)
        return;

    // Half-texel inset keeps bilinear repeat sampling from bleeding the opposite V edge
    if (texture && texture->GetSize().y > 0.0f)
    {
        float halfTexel = 0.5f / texture->GetSize().y;
        vA = Math::Clamp(vA, halfTexel, 1.0f - halfTexel);
        vB = Math::Clamp(vB, halfTexel, 1.0f - halfTexel);
    }

    auto normalAt = [&](int idx) {
        Vec2F prevP = idx == 0 ? path[n - 2] : path[idx - 1];
        Vec2F nextP = idx == n - 1 ? path[1] : path[idx + 1];
        Vec2F t = nextP - prevP;
        if (t.Length() > 0.0001f)
            t.Normalize();

        return Vec2F(-t.y, t.x) * inwardSign;
    };

    mesh.Resize(n * 2, (n - 1) * 2);

    UInt32 color = colorValue.ARGB();
    float accumLen = 0.0f;

    Vertex* verts = mesh.GetVertices<Vertex>();
    for (int i = 0; i < n; i++)
    {
        if (i > 0)
            accumLen += (path[i] - path[i - 1]).Length();

        Vec2F inward = normalAt(i);
        Vec2F a = (path[i] + inward * offsetA + shift) * transform;
        Vec2F b = (path[i] + inward * offsetB + shift) * transform;

        float u = accumLen / tileLength;
        verts[i * 2 + 0].Set(a, 1.0f, color, u, vA);
        verts[i * 2 + 1].Set(b, 1.0f, color, u, vB);
    }

    VertexIndex* idx = mesh.GetIndexes();
    int triIdx = 0;
    for (int i = 1; i < n; i++)
    {
        int a0 = (i - 1) * 2 + 0;
        int b0 = (i - 1) * 2 + 1;
        int a1 = i * 2 + 0;
        int b1 = i * 2 + 1;

        idx[triIdx * 3 + 0] = a0;
        idx[triIdx * 3 + 1] = b0;
        idx[triIdx * 3 + 2] = a1;
        triIdx++;
        idx[triIdx * 3 + 0] = b0;
        idx[triIdx * 3 + 1] = b1;
        idx[triIdx * 3 + 2] = a1;
        triIdx++;
    }

    mesh.SetTexture(texture);
    mesh.vertexCount = n * 2;
    mesh.polyCount = (n - 1) * 2;
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<GameFieldBorder>);
// --- META ---

DECLARE_CLASS(GameFieldBorder, GameFieldBorder);
// --- END META ---
