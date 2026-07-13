#include "o2/stdafx.h"
#include "SlingRubber.h"

#include "o2/Render/Render.h"
#include "o2/Utils/Math/Math.h"
#include "o2/Utils/Math/Vertex.h"

namespace
{
	// Tangent point where a band anchored at `post` first touches a chip (centre `c`, radius `r`),
	// choosing the contact on the field-facing side `n` so the band wraps the front of the chip.
	Vec2F TangentPoint(const Vec2F& post, const Vec2F& c, float r, const Vec2F& n)
	{
		Vec2F v = post - c;
		float d = v.Length();
		if (d <= r + 1.0f)
			return c + v * (r / Math::Max(d, 0.001f));

		float phi = Math::Atan2F(v.y, v.x);
		float alpha = Math::ACos(Math::Clamp(r / d, -1.0f, 1.0f));

		Vec2F t1 = c + Vec2F(Math::Cos(phi + alpha), Math::Sin(phi + alpha)) * r;
		Vec2F t2 = c + Vec2F(Math::Cos(phi - alpha), Math::Sin(phi - alpha)) * r;

		return (t1 - c).Dot(n) >= (t2 - c).Dot(n) ? t1 : t2;
	}
}

void SlingRubber::SetGrip(const Vec2F& grip, float chipRadius /*= 34.0f*/)
{
	mGrip = grip;
	mChipRadius = chipRadius;
	mActive = true;
}

void SlingRubber::ClearGrip()
{
	mActive = false;
}

Vec2F SlingRubber::ClampGripToBack(const Vec2F& grip, int side, float restY)
{
	Vec2F clamped = grip;
	if (side == 0)
		clamped.y = Math::Min(clamped.y, restY); // player band bends downward only
	else
		clamped.y = Math::Max(clamped.y, restY); // bot band bends upward only

	return clamped;
}

Vec2F SlingRubber::GetEffectiveGrip() const
{
	if (!mActive)
		return Vec2F(0.0f, restY);

	return ClampGripToBack(mGrip, side, restY);
}

Vec2F SlingRubber::ComputeLaunch(const Vec2F& grip, float power, float maxSpeed) const
{
	Vec2F pulled = ClampGripToBack(grip, side, restY);
	float depth = side == 0 ? (restY - pulled.y) : (pulled.y - restY); // how far behind the band
	if (depth < minStretch)
		return Vec2F();

	// Gentle lateral aim, capped so the shot never deviates from straight forward by more
	// than maxAimAngle: a pull at the field edge can't fling the chip sharply sideways
	float aimRad = Math::Deg2rad(maxAimAngle);
	float maxLateral = depth * (Math::Sin(aimRad) / Math::Max(Math::Cos(aimRad), 0.001f));
	float lateral = Math::Clamp(-pulled.x * lateralAim, -maxLateral, maxLateral);

	float forward = side == 0 ? depth : -depth; // band normal, into the field
	Vec2F launch(lateral * power, forward * power);

	float len = launch.Length();
	if (len > maxSpeed && len > 0.0f)
		launch = launch * (maxSpeed / len);

	return launch;
}

Vector<Vec2F> SlingRubber::BuildBandPath(const Vec2F& grip, float chipRadius, int arcSegments) const
{
	Vec2F leftPost(-halfSpan, restY);
	Vec2F rightPost(halfSpan, restY);

	Vec2F c = ClampGripToBack(grip, side, restY);
	float depth = side == 0 ? (restY - c.y) : (c.y - restY);

	Vector<Vec2F> path;
	if (depth < minStretch || chipRadius <= 0.0f)
	{
		path.Add(leftPost);
		path.Add(rightPost);
		return path;
	}

	// The band cradles the chip on its back side (away from the field), like a slingshot pouch;
	// the posts sit on the field side, so these tangents never cross and the legs stay taut.
	Vec2F n(0.0f, side == 0 ? -1.0f : 1.0f);
	Vec2F tl = TangentPoint(leftPost, c, chipRadius, n);
	Vec2F tr = TangentPoint(rightPost, c, chipRadius, n);

	float aL = Math::Atan2F(tl.y - c.y, tl.x - c.x);
	float aR = Math::Atan2F(tr.y - c.y, tr.x - c.x);
	float twoPi = 2.0f * Math::PI();
	float dA = aR - aL;
	while (dA > Math::PI()) dA -= twoPi;   // sweep the short way, which stays on the field side
	while (dA < -Math::PI()) dA += twoPi;

	int segs = Math::Max(arcSegments, 1);
	path.Add(leftPost);
	path.Add(tl);
	for (int i = 1; i < segs; i++)
	{
		float a = aL + dA * (float)i / (float)segs;
		path.Add(c + Vec2F(Math::Cos(a), Math::Sin(a)) * chipRadius);
	}
	path.Add(tr);
	path.Add(rightPost);
	return path;
}

void SlingRubber::OnDraw()
{
	Vec2F grip = mActive ? mGrip : Vec2F(0.0f, restY);
	Vector<Vec2F> path = BuildBandPath(grip, mChipRadius, 24);

	const int maxPoints = 64;
	int count = Math::Min(path.Count(), maxPoints);
	if (count < 2)
		return;

	if (!mBandTexture)
		mBandTexture = AssetRef<ImageAsset>(side == 0 ? String("rubber_red.png") : String("rubber_blue.png"));

	TextureSource src = mBandTexture ? mBandTexture->GetTextureSource() : TextureSource();
	RectI srcRect = src.sourceRect;

	float totalLen = 0.0f;
	for (int i = 1; i < count; i++)
		totalLen += (path[i] - path[i - 1]).Length();
	if (totalLen <= 0.0f)
		return;

	// Thick strip along the band centerline; u runs end-to-end so the strip art stretches over the
	// whole span, v crosses the thickness.
	Color32Bit c = Color4::White().ABGR();
	float half = thickness * 0.5f;

	Vertex verts[maxPoints * 2];
	float u = 0.0f;
	for (int i = 0; i < count; i++)
	{
		Vec2F tangent = i == 0           ? path[1] - path[0]
					  : i == count - 1   ? path[count - 1] - path[count - 2]
										 : path[i + 1] - path[i - 1];
		Vec2F n = tangent.Normalized().Perpendicular();

		if (i > 0)
			u += (path[i] - path[i - 1]).Length() / totalLen;

		verts[i * 2 + 0] = Vertex(path[i] + n * half, c, u, 0.0f);
		verts[i * 2 + 1] = Vertex(path[i] - n * half, c, u, 1.0f);
	}

	VertexIndex idx[(maxPoints - 1) * 6];
	int ii = 0;
	for (int i = 0; i < count - 1; i++)
	{
		int a = i * 2;
		idx[ii++] = a;     idx[ii++] = a + 1; idx[ii++] = a + 3;
		idx[ii++] = a;     idx[ii++] = a + 3; idx[ii++] = a + 2;
	}

	o2Render.DrawBuffer(PrimitiveType::Polygon, verts, count * 2, idx, ii / 3,
						nullptr, src.texture, srcRect, true);
}

DECLARE_TEMPLATE_CLASS(o2::LinkRef<SlingRubber>);
// --- META ---

DECLARE_CLASS(SlingRubber, SlingRubber);
// --- END META ---
