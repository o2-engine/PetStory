#pragma once

#include "o2/Assets/Types/ImageAsset.h"
#include "o2/Render/Mesh.h"
#include "o2/Render/TextureRef.h"
#include "o2/Scene/Physics/SplineMeshCollider.h"

using namespace o2;

// ---------------------------------------------------------------------
// Game field border: a closed spline that forms both the physical wall
// of the field (inherited SplineMeshCollider loop chain + tiling beige
// border strip) and the field graphics: a tiled background polygon,
// an inner shadow strip along the border and a drop shadow strip
// under everything, following the field contour.
// ---------------------------------------------------------------------
class GameFieldBorder: public SplineMeshCollider
{
public:
    PROPERTIES(GameFieldBorder);
    PROPERTY(AssetRef<ImageAsset>, backImage, SetBackImage, GetBackImage);          // Tiled background image
    PROPERTY(float, backTileSize, SetBackTileSize, GetBackTileSize);                // World size of one background tile
    PROPERTY(AssetRef<ImageAsset>, shadowImage, SetShadowImage, GetShadowImage);    // Shadow gradient strip image
    PROPERTY(float, innerShadowWidth, SetInnerShadowWidth, GetInnerShadowWidth);    // Inner shadow width along border
    PROPERTY(float, dropShadowWidth, SetDropShadowWidth, GetDropShadowWidth);       // Drop shadow width outside border
    PROPERTY(Vec2F, dropShadowOffset, SetDropShadowOffset, GetDropShadowOffset);    // Drop shadow shift

public:
    GameFieldBorder();
    GameFieldBorder(const GameFieldBorder& other);
    ~GameFieldBorder();

    GameFieldBorder& operator=(const GameFieldBorder& other);

    void SetBackImage(const AssetRef<ImageAsset>& image);
    const AssetRef<ImageAsset>& GetBackImage() const;

    void SetBackTileSize(float size);
    float GetBackTileSize() const;

    void SetShadowImage(const AssetRef<ImageAsset>& image);
    const AssetRef<ImageAsset>& GetShadowImage() const;

    void SetInnerShadowWidth(float width);
    float GetInnerShadowWidth() const;

    void SetDropShadowWidth(float width);
    float GetDropShadowWidth() const;

    void SetDropShadowOffset(const Vec2F& offset);
    const Vec2F& GetDropShadowOffset() const;

    // Samples the closed spline into a local-space polygon; last point equals first
    Vector<Vec2F> GetLocalPolygon() const;

    // Rebuilds background, shadows and border meshes immediately
    void RebuildFieldMeshes();

    const Mesh& GetBackMesh() const;
    const Mesh& GetInnerShadowMesh() const;
    const Mesh& GetDropShadowMesh() const;
    const Mesh& GetBorderMesh() const;

    static String GetName();
    static String GetCategory();
    static bool IsAvailableFromCreateMenu();

    SERIALIZABLE(GameFieldBorder);
    CLONEABLE_REF(GameFieldBorder);

protected:
    AssetRef<ImageAsset> mBackImage;                    // Tiled background image @SERIALIZABLE
    float                mBackTileSize = 512.0f;        // World size of one background tile @SERIALIZABLE
    AssetRef<ImageAsset> mShadowImage;                  // Shadow gradient strip image @SERIALIZABLE
    float                mInnerShadowWidth = 70.0f;     // Inner shadow width @SERIALIZABLE
    float                mDropShadowWidth = 60.0f;      // Drop shadow width @SERIALIZABLE
    Vec2F                mDropShadowOffset = Vec2F(0.0f, -15.0f); // Drop shadow shift @SERIALIZABLE

    Mesh mBackMesh;        // Triangulated tiled background polygon
    Mesh mInnerShadowMesh; // Inner shadow strip, drawn above background
    Mesh mDropShadowMesh;  // Drop shadow strip, drawn under everything
    Mesh mBorderMesh;      // Border strip, drawn on top

    TextureRef mBackTexture;   // Standalone repeat-wrap texture of background
    TextureRef mShadowTexture; // Standalone repeat-wrap texture of shadow
    TextureRef mBorderTexture; // Standalone repeat-wrap texture of border

    AssetRef<ImageAsset> mBorderTextureImage; // Image the border texture was created from

    bool mNeedUpdateFieldMeshes = true;   // True when field meshes must be rebuilt before next draw
    bool mDefaultAssetsChecked = false;   // True after default images were assigned once

protected:
    // Override: also marks field meshes dirty
    void OnSplineChanged() override;

    // Draws drop shadow, background, inner shadow, then the inherited border strip
    void OnDraw() override;

    void OnTransformUpdated() override;
    void OnAddToScene() override;

    // Assigns default field images when properties are empty
    void EnsureDefaultAssets();

    // Creates standalone repeat-wrap texture from image asset bitmap
    TextureRef CreateRepeatTexture(const AssetRef<ImageAsset>& image) const;

    // Rebuilds mBackMesh / mInnerShadowMesh / mDropShadowMesh from the spline
    void UpdateFieldMeshes();

    // Builds the triangulated tiled background polygon
    void BuildBackMesh(const Vector<Vec2F>& path, const Basis& transform);

    // Builds a textured strip along the closed path between two offsets along the inward normal
    void BuildStripMesh(Mesh& mesh, const Vector<Vec2F>& path, const Basis& transform, float inwardSign,
                        float offsetA, float offsetB, const Vec2F& shift, const TextureRef& texture,
                        float tileLength, float vA, float vB, const Color4& color);
};
// --- META ---

CLASS_BASES_META(GameFieldBorder)
{
    BASE_CLASS(SplineMeshCollider);
}
END_META;
CLASS_FIELDS_META(GameFieldBorder)
{
    FIELD().PUBLIC().NAME(backImage);
    FIELD().PUBLIC().NAME(backTileSize);
    FIELD().PUBLIC().NAME(shadowImage);
    FIELD().PUBLIC().NAME(innerShadowWidth);
    FIELD().PUBLIC().NAME(dropShadowWidth);
    FIELD().PUBLIC().NAME(dropShadowOffset);
    FIELD().PROTECTED().SERIALIZABLE_ATTRIBUTE().NAME(mBackImage);
    FIELD().PROTECTED().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(512.0f).NAME(mBackTileSize);
    FIELD().PROTECTED().SERIALIZABLE_ATTRIBUTE().NAME(mShadowImage);
    FIELD().PROTECTED().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(70.0f).NAME(mInnerShadowWidth);
    FIELD().PROTECTED().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(60.0f).NAME(mDropShadowWidth);
    FIELD().PROTECTED().SERIALIZABLE_ATTRIBUTE().DEFAULT_VALUE(Vec2F(0.0f, -15.0f)).NAME(mDropShadowOffset);
    FIELD().PROTECTED().NAME(mBackMesh);
    FIELD().PROTECTED().NAME(mInnerShadowMesh);
    FIELD().PROTECTED().NAME(mDropShadowMesh);
    FIELD().PROTECTED().NAME(mBorderMesh);
    FIELD().PROTECTED().NAME(mBackTexture);
    FIELD().PROTECTED().NAME(mShadowTexture);
    FIELD().PROTECTED().NAME(mBorderTexture);
    FIELD().PROTECTED().NAME(mBorderTextureImage);
    FIELD().PROTECTED().DEFAULT_VALUE(true).NAME(mNeedUpdateFieldMeshes);
    FIELD().PROTECTED().DEFAULT_VALUE(false).NAME(mDefaultAssetsChecked);
}
END_META;
CLASS_METHODS_META(GameFieldBorder)
{

    FUNCTION().PUBLIC().CONSTRUCTOR();
    FUNCTION().PUBLIC().CONSTRUCTOR(const GameFieldBorder&);
    FUNCTION().PUBLIC().SIGNATURE(void, SetBackImage, const AssetRef<ImageAsset>&);
    FUNCTION().PUBLIC().SIGNATURE(const AssetRef<ImageAsset>&, GetBackImage);
    FUNCTION().PUBLIC().SIGNATURE(void, SetBackTileSize, float);
    FUNCTION().PUBLIC().SIGNATURE(float, GetBackTileSize);
    FUNCTION().PUBLIC().SIGNATURE(void, SetShadowImage, const AssetRef<ImageAsset>&);
    FUNCTION().PUBLIC().SIGNATURE(const AssetRef<ImageAsset>&, GetShadowImage);
    FUNCTION().PUBLIC().SIGNATURE(void, SetInnerShadowWidth, float);
    FUNCTION().PUBLIC().SIGNATURE(float, GetInnerShadowWidth);
    FUNCTION().PUBLIC().SIGNATURE(void, SetDropShadowWidth, float);
    FUNCTION().PUBLIC().SIGNATURE(float, GetDropShadowWidth);
    FUNCTION().PUBLIC().SIGNATURE(void, SetDropShadowOffset, const Vec2F&);
    FUNCTION().PUBLIC().SIGNATURE(const Vec2F&, GetDropShadowOffset);
    FUNCTION().PUBLIC().SIGNATURE(Vector<Vec2F>, GetLocalPolygon);
    FUNCTION().PUBLIC().SIGNATURE(void, RebuildFieldMeshes);
    FUNCTION().PUBLIC().SIGNATURE(const Mesh&, GetBackMesh);
    FUNCTION().PUBLIC().SIGNATURE(const Mesh&, GetInnerShadowMesh);
    FUNCTION().PUBLIC().SIGNATURE(const Mesh&, GetDropShadowMesh);
    FUNCTION().PUBLIC().SIGNATURE(const Mesh&, GetBorderMesh);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(String, GetName);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(String, GetCategory);
    FUNCTION().PUBLIC().SIGNATURE_STATIC(bool, IsAvailableFromCreateMenu);
    FUNCTION().PROTECTED().SIGNATURE(void, OnSplineChanged);
    FUNCTION().PROTECTED().SIGNATURE(void, OnDraw);
    FUNCTION().PROTECTED().SIGNATURE(void, OnTransformUpdated);
    FUNCTION().PROTECTED().SIGNATURE(void, OnAddToScene);
    FUNCTION().PROTECTED().SIGNATURE(void, EnsureDefaultAssets);
    FUNCTION().PROTECTED().SIGNATURE(TextureRef, CreateRepeatTexture, const AssetRef<ImageAsset>&);
    FUNCTION().PROTECTED().SIGNATURE(void, UpdateFieldMeshes);
    FUNCTION().PROTECTED().SIGNATURE(void, BuildBackMesh, const Vector<Vec2F>&, const Basis&);
    FUNCTION().PROTECTED().SIGNATURE(void, BuildStripMesh, Mesh&, const Vector<Vec2F>&, const Basis&, float, float, float, const Vec2F&, const TextureRef&, float, float, float, const Color4&);
}
END_META;
// --- END META ---
