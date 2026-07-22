#!/usr/bin/env python3
"""Normal-map lighting for the free-form field objects (Assets/Game field/Objects).

Per object a normal map is built from distance fields: the silhouette DF gives an
outer bevel, and color-segmented PARTS (acorn cap, brush handle, ...) give inner
bevels, so the surface reads as a set of rounded pieces. The shader keeps the
reference art as-is and modulates it gently:
    shade = 1 + k * dot(N.xy, L(angle) - L(0))
so at 0 deg the source art is untouched and under rotation the lighting flows
organically across the parts. No bright highlights, no hard shadows.

The painted directional light is partially moved OUT of the art: the sprite becomes
{name}_alb.png = art / (1 + k*dot(N, L0)), and the shader multiplies the same factor
back with the rotated light — at 0 deg the original art is reproduced exactly, under
rotation the very same shading follows the world light. Normals carry a luminance
detail layer so the reproduced lighting keeps the painted detail.

Outputs: {name}_nrm.png (RG = normal.xy, B = silhouette DF, A = 255) and
{name}_alb.png into the assets folder; preview strips into Out/.
"""
import numpy as np
from PIL import Image
import os, sys, json

SP = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SP)
from pipeline import gauss, chamfer_dt
from sdf import smoothstep

OBJ = "/Users/andreizenkovich/work/PetStory/Assets/Game field/Objects"
OUT = "/Users/andreizenkovich/work/PetStory/Out"
NAMES = ["acorn", "bandage", "bone", "brush", "food", "leaf", "patch", "pill",
         "pillow", "soap", "water", "wood"]

UNIFIED_LIGHT = np.array([-0.3292, 0.9443], np.float32)
DF_RANGE = 64.0        # px, encoded half-range for the B channel
BODY_SIGMA = 26.0      # px, pillow-inflation blur of the silhouette
BODY_AMP = 0.72
PART_SIGMA = 12.0      # px, inflation blur of the parts
PART_AMP = 0.33
PART_K = 3             # color clusters per object
PART_OVERRIDE = {"bone": 0, "pill": 0}  # near-monochrome art: painted shading is not parts
DETAIL_SIGMA = 3.0     # px, luminance detail blur for the fine normal layer
DETAIL_AMP = 0.30
DIFF_K = 0.5           # shader lighting strength (absolute: shade = 1 + k*dot(N, L))
K_OVERRIDE = {"water": 0.25}  # saturated art clips at the gamut ceiling when de-lit
SHADOW_DARKEN = 0.78   # shadow tint = dark-quartile median of the art, slightly darkened
SHADE_MIN, SHADE_MAX = 0.6, 1.7  # de-light clamp


def load_rgba(path):
    return np.asarray(Image.open(path).convert("RGBA")).astype(np.float32)/255.0


def to_u8(img):
    return Image.fromarray((np.clip(img, 0, 1)*255 + 0.5).astype(np.uint8))


def signed_df(mask):
    return chamfer_dt(mask) - chamfer_dt(1.0 - mask)


def dome(mask, sigma, amp):
    """Pillow inflation: the heavily blurred mask is a smooth height dome over the
    whole shape, so its gradient spreads from edge to center — no edge band."""
    h = gauss(mask, sigma)
    gy, gx = np.gradient(h)
    g = np.stack([gx, gy], -1)
    peak = np.abs(g).max()
    return -g/max(peak, 1e-6)*amp


def color_parts(ref, mask, k=PART_K):
    """K-means on color inside the silhouette; returns list of smoothed part masks."""
    rgb = ref[..., :3]
    pts = rgb[mask > 0.5].reshape(-1, 3)
    rng = np.random.default_rng(7)
    centers = pts[rng.choice(len(pts), k, replace=False)]
    for _ in range(12):
        d = ((pts[:, None, :] - centers[None])**2).sum(-1)
        lab = d.argmin(1)
        for c in range(k):
            sel = pts[lab == c]
            if len(sel):
                centers[c] = sel.mean(0)
    # merge clusters with close colors: near-monochrome art must not split by its
    # painted shading (that would re-emboss the shading as fake parts)
    groups = list(range(k))
    for i in range(k):
        for j in range(i):
            if np.linalg.norm(centers[i] - centers[j]) < 0.16:
                groups[i] = groups[j]
    full = ((rgb[..., None, :] - centers[None, None])**2).sum(-1).argmin(-1)
    parts = []
    for g in sorted(set(groups)):
        sel = np.zeros_like(full, bool)
        for c in range(k):
            if groups[c] == g:
                sel |= (full == c)
        pm = (sel & (mask > 0.5)).astype(np.float32)
        pm = (gauss(pm, 3.0) > 0.5).astype(np.float32)  # despeckle
        if pm.sum() > 200:
            parts.append(pm)
    return parts if len(parts) > 1 else []  # a single part duplicates the outer bevel


def build(name):
    ref = load_rgba(f"{OBJ}/{name}.png")
    h, w = ref.shape[:2]
    a = ref[..., 3]
    mask = (a > 0.5).astype(np.float32)
    sd = gauss(signed_df(mask), 2.0)

    nxy = dome(mask, BODY_SIGMA, BODY_AMP)
    for pm in (color_parts(ref, mask) if PART_OVERRIDE.get(name, PART_K) else []):
        nxy = nxy + dome(pm, PART_SIGMA, PART_AMP)

    # fine detail from the art itself: bright = high, so the reproduced lighting
    # keeps the painted bumps (kibbles, seams, scales)
    lum = gauss(ref[..., :3].mean(-1)*mask, DETAIL_SIGMA)
    gy, gx = np.gradient(lum)
    gd = np.stack([gx, gy], -1)
    peak = np.percentile(np.abs(gd), 99.5)
    nxy = nxy - gd/max(peak, 1e-6)*DETAIL_AMP*mask[..., None]

    nxy = gauss(nxy, 1.0)
    l = np.linalg.norm(nxy, axis=-1, keepdims=True)
    over = np.maximum(l, 0.94)
    nxy = nxy/over*0.94  # keep some z everywhere

    # sprite space is y-up: flip the image-space y component
    out = np.zeros((h, w, 4), np.float32)
    out[..., 0] = nxy[..., 0]*0.5 + 0.5
    out[..., 1] = -nxy[..., 1]*0.5 + 0.5
    out[..., 2] = np.clip(0.5 + sd/(2*DF_RANGE), 0, 1)
    out[..., 3] = 1.0
    to_u8(out).save(f"{OBJ}/{name}_nrm.png")

    # shadow tint: the object's own hue instead of black — median of the darker
    # quartile of the art, slightly darkened
    lum = ref[..., :3].mean(-1)
    inside = mask > 0.5
    q25 = np.quantile(lum[inside], 0.25)
    sc = np.median(ref[..., :3][inside & (lum <= q25)].reshape(-1, 3), 0)*SHADOW_DARKEN
    sc = np.clip(sc, 0.06, 1.0)

    # de-light: invert exactly what the shader applies at 0 deg
    # (lit side multiplies, dark side mixes toward the shadow tint)
    nq = np.asarray(Image.open(f"{OBJ}/{name}_nrm.png")).astype(np.float32)/255.0
    nxy_q = np.stack([nq[..., 0]*2 - 1, -(nq[..., 1]*2 - 1)], -1)  # back to image y-down
    L_img = np.array([UNIFIED_LIGHT[0], -UNIFIED_LIGHT[1]], np.float32)
    k = K_OVERRIDE.get(name, DIFF_K)
    d0 = nxy_q @ L_img
    shade0 = np.clip(1.0 + k*np.maximum(d0, 0.0), 1.0, SHADE_MAX)
    sh0 = np.clip(k*np.maximum(-d0, 0.0), 0.0, 1.0 - SHADE_MIN)
    alb = (ref[..., :3] - sc[None, None, :]*sh0[..., None]) \
        / np.maximum(shade0[..., None]*(1.0 - sh0[..., None]), 1e-3)
    to_u8(np.concatenate([np.clip(alb, 0, 1), ref[..., 3:4]], -1)).save(f"{OBJ}/{name}_alb.png")
    json.dump(dict(shadowColor=[round(float(x), 4) for x in sc]),
              open(f"{SP}/{name}_obj.json", "w"))
    return out


def render_preview(name, angle_deg):
    """Twin of the shader: albedo relit by the world light rotated into sprite space."""
    alb = load_rgba(f"{OBJ}/{name}_alb.png")
    nrm = np.asarray(Image.open(f"{OBJ}/{name}_nrm.png")).astype(np.float32)/255.0
    nxy = np.stack([nrm[..., 0]*2 - 1, nrm[..., 1]*2 - 1], -1)  # sprite space, y up

    th = np.deg2rad(angle_deg)
    c, s = np.cos(-th), np.sin(-th)
    L = UNIFIED_LIGHT
    Ls = np.array([L[0]*c - L[1]*s, L[0]*s + L[1]*c], np.float32)
    k = K_OVERRIDE.get(name, DIFF_K)
    sc = np.array(json.load(open(f"{SP}/{name}_obj.json"))["shadowColor"], np.float32)
    d = nxy @ Ls
    shade = np.clip(1.0 + k*np.maximum(d, 0.0), 1.0, SHADE_MAX)
    sh = np.clip(k*np.maximum(-d, 0.0), 0.0, 1.0 - SHADE_MIN)
    color = alb[..., :3]*shade[..., None]*(1.0 - sh[..., None]) + sc[None, None, :]*sh[..., None]
    return np.concatenate([np.clip(color, 0, 1), alb[..., 3:4]], -1)


def rotate_screen(img_rgba, angle_deg):
    im = Image.fromarray((np.clip(img_rgba, 0, 1)*255).astype(np.uint8))
    return im.rotate(angle_deg, Image.BICUBIC, expand=True)


def preview_sheet(names, angles=(0, 45, 90, 135, 180, 270), cell=190):
    """Rows = objects, columns = rotation angles, composed the way they appear on screen."""
    sheet = Image.new("RGB", (cell*len(angles), cell*len(names) + 22), (28, 28, 32))
    from PIL import ImageDraw
    d = ImageDraw.Draw(sheet)
    for ai, ang in enumerate(angles):
        d.text((ai*cell + 8, 4), f"{ang} deg", fill=(190, 190, 190))
    for ni, name in enumerate(names):
        for ai, ang in enumerate(angles):
            im = rotate_screen(render_preview(name, ang), ang)
            im.thumbnail((cell - 8, cell - 8))
            bg = Image.new("RGBA", (cell, cell), (28, 28, 32, 255))
            bg.paste(im, ((cell - im.width)//2, (cell - im.height)//2), im)
            sheet.paste(bg.convert("RGB"), (ai*cell, 22 + ni*cell))
    return sheet


if __name__ == "__main__":
    names = sys.argv[1:] or NAMES
    for n in names:
        build(n)
        print(n, "built")
    preview_sheet(names).save(f"{OUT}/objects_normal_preview.png")
    print("preview saved")
