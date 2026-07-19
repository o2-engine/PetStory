#!/usr/bin/env python3
"""Builds chip albedo+normal textures from MCP outputs and simulates the ChipLit shader."""
import numpy as np
from PIL import Image, ImageFilter
import os, json, sys

SP = os.path.dirname(os.path.abspath(__file__))
ASSETS = "/Users/andreizenkovich/work/PetStory/Assets"
REFS = {
    "red":  f"{ASSETS}/Game field/Objects/Main/red.png",
    "blue": f"{ASSETS}/Game field/Objects/Main/blue.png",
    "leaf": f"{ASSETS}/Game field/Objects/leaf.png",
}
WORK = 512  # internal supersample max dimension


def load_rgba(path, size=None):
    img = Image.open(path).convert("RGBA")
    if size is not None:
        img = img.resize(size, Image.LANCZOS)
    return np.asarray(img).astype(np.float32) / 255.0


def to_img(arr):
    return Image.fromarray((np.clip(arr, 0, 1) * 255).astype(np.uint8))


def gauss(arr, sigma):
    """Float separable Gaussian blur (no 8-bit roundtrip)."""
    if sigma <= 0:
        return arr.astype(np.float32)
    radius = max(1, int(sigma * 3))
    x = np.arange(-radius, radius + 1, dtype=np.float32)
    k = np.exp(-0.5 * (x / sigma) ** 2)
    k /= k.sum()
    out = arr.astype(np.float32)
    out = np.apply_along_axis(lambda m: np.convolve(np.pad(m, radius, mode="edge"), k, "valid"), 0, out)
    out = np.apply_along_axis(lambda m: np.convolve(np.pad(m, radius, mode="edge"), k, "valid"), 1, out)
    return out


def work_size(ref_size):
    w, h = ref_size
    s = WORK / max(w, h)
    return (round(w * s), round(h * s))


def chamfer_dt(mask):
    """3-4 chamfer distance transform inside mask (positive inside)."""
    h, w = mask.shape
    INF = 1e9
    d = np.where(mask > 0.5, INF, 0.0).astype(np.float32)
    for y in range(h):
        row = d[y]
        up = d[y - 1] if y > 0 else None
        for x in range(w):
            v = row[x]
            if v == 0:
                continue
            if x > 0:
                v = min(v, row[x - 1] + 3)
            if up is not None:
                v = min(v, up[x] + 3)
                if x > 0:
                    v = min(v, up[x - 1] + 4)
                if x < w - 1:
                    v = min(v, up[x + 1] + 4)
            row[x] = v
    for y in range(h - 1, -1, -1):
        row = d[y]
        dn = d[y + 1] if y < h - 1 else None
        for x in range(w - 1, -1, -1):
            v = row[x]
            if v == 0:
                continue
            if x < w - 1:
                v = min(v, row[x + 1] + 3)
            if dn is not None:
                v = min(v, dn[x] + 3)
                if x < w - 1:
                    v = min(v, dn[x + 1] + 4)
                if x > 0:
                    v = min(v, dn[x - 1] + 4)
            row[x] = v
    return d / 3.0


def clean_mask(mask, drop_border=True):
    """Keep components not touching the border (simple flood fill labeling)."""
    from collections import deque
    h, w = mask.shape
    binm = mask > 0.5
    seen = np.zeros((h, w), bool)
    out = np.zeros((h, w), np.float32)
    for sy in range(h):
        for sx in range(w):
            if binm[sy, sx] and not seen[sy, sx]:
                q = deque([(sy, sx)])
                seen[sy, sx] = True
                comp = []
                touches = False
                while q:
                    y, x = q.popleft()
                    comp.append((y, x))
                    if y in (0, h - 1) or x in (0, w - 1):
                        touches = True
                    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        ny, nx = y + dy, x + dx
                        if 0 <= ny < h and 0 <= nx < w and binm[ny, nx] and not seen[ny, nx]:
                            seen[ny, nx] = True
                            q.append((ny, nx))
                if not (drop_border and touches):
                    for y, x in comp:
                        out[y, x] = 1.0
    return out


def fill_holes(alpha, threshold=0.3):
    """Solid silhouette: interior semi-transparent brush texture becomes opaque."""
    from collections import deque
    solid = alpha > threshold
    h, w = solid.shape
    outside = np.zeros((h, w), bool)
    q = deque()
    for x in range(w):
        for y in (0, h - 1):
            if not solid[y, x] and not outside[y, x]:
                outside[y, x] = True
                q.append((y, x))
    for y in range(h):
        for x in (0, w - 1):
            if not solid[y, x] and not outside[y, x]:
                outside[y, x] = True
                q.append((y, x))
    while q:
        y, x = q.popleft()
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            ny, nx = y + dy, x + dx
            if 0 <= ny < h and 0 <= nx < w and not solid[ny, nx] and not outside[ny, nx]:
                outside[ny, nx] = True
                q.append((ny, nx))
    filled = np.where(~outside, 1.0, alpha).astype(np.float32)
    return filled


def dilate_rgb(rgb, alpha, iters=24):
    """Push valid colors outward so filtering across the alpha edge stays clean."""
    valid = alpha > 0.05
    out = rgb.copy()
    for _ in range(iters):
        grown = valid.copy()
        acc = np.zeros_like(out)
        cnt = np.zeros(valid.shape, np.float32)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            sv = np.roll(valid, (dy, dx), (0, 1))
            sc = np.roll(out, (dy, dx), (0, 1))
            add = sv & ~valid
            acc[add] += sc[add]
            cnt[add] += 1
            grown |= sv
        fill = (cnt > 0) & ~valid
        out[fill] = acc[fill] / cnt[fill][:, None]
        valid = grown
        if valid.all():
            break
    return out


def height_to_normal(height, strength):
    gy, gx = np.gradient(height)
    nx = -gx * strength
    ny = gy * strength  # +y up in world; image y grows down
    nz = np.ones_like(height)
    n = np.stack([nx, ny, nz], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n


def encode_normal(n, alpha):
    rgb = n * 0.5 + 0.5
    rgb = dilate_rgb(rgb, alpha)
    # dither before 8-bit quantization: kills specular contour banding on near-flat relief
    rng = np.random.default_rng(7)
    rgb = np.clip(rgb + (rng.random(rgb.shape).astype(np.float32) - 0.5) / 255.0, 0, 1)
    out = np.concatenate([rgb, alpha[..., None]], -1)
    return out


def build_ball(name, emboss_amp=13.0, emboss_sigma=8.0, dome_scale=95.0, flatten=0.72,
               profile="hemisphere", bevel_start=0.78, emboss_style="plateau", super_pow=4):
    ref = Image.open(REFS[name])
    wsize = work_size(ref.size)
    ref_a = fill_holes(load_rgba(REFS[name], wsize)[..., 3])
    albedo_rgb = load_rgba(f"{SP}/{name}_albedo_raw.png", wsize)[..., :3]
    inner = load_rgba(f"{SP}/{name}_inner_mask.png", wsize)[..., :3].mean(-1)
    inner = clean_mask(inner)
    inner = gauss(inner, 2)

    mask = ref_a > 0.5
    ys, xs = np.nonzero(mask)
    cy, cx = ys.mean(), xs.mean()
    R = max(np.hypot(ys - cy, xs - cx).max(), 1.0)
    yy, xx = np.mgrid[0:wsize[1], 0:wsize[0]]
    r = np.hypot(yy - cy, xx - cx) / R
    if profile == "plate":
        # flat plateau with a narrow round bevel: candy-button look, thin edge shading ring
        t = np.clip((np.minimum(r, 1.0) - bevel_start) / (1.0 - bevel_start), 0, 1)
        dome = np.sqrt(np.clip(1.0 - t ** 2, 0, 1)) * dome_scale
    elif profile == "super":
        # superellipse dome: nearly flat interior with gentle tilt, curvature packed at the edge
        dome = np.sqrt(np.clip(1.0 - np.minimum(r, 1.0) ** super_pow, 0, 1)) * dome_scale
    else:
        dome = np.sqrt(np.clip(1.0 - np.minimum(r, 1.0) ** 2, 0, 1)) ** flatten * dome_scale

    if emboss_style == "pillow":
        # puffy shape: height from distance-to-shape-edge, crisp curvature turn at the border
        idt = chamfer_dt(np.where(inner > 0.5, 1.0, 0.0).astype(np.float32))
        pil = (idt / max(idt.max(), 1.0)) ** 0.5
        height = dome + gauss(pil, 2.0) * emboss_amp
    else:
        height = dome + gauss(inner, emboss_sigma) * emboss_amp
    if name == "red":  # the red reference blob has a wrinkled, hand-molded surface
        rng = np.random.default_rng(11)
        wrinkles = gauss(rng.random(height.shape).astype(np.float32) - 0.5, 11.0)
        height += wrinkles * 65.0 * gauss(inner, 3)
    normal = height_to_normal(gauss(height / max(dome_scale, 1e-3), 1.5) * dome_scale, 1.0)

    # synthetic flat albedo: median base + median inner tone, soft blended by the inner mask
    inner_bin = inner > 0.5
    base_px = albedo_rgb[mask & ~inner_bin]
    inner_px = albedo_rgb[mask & inner_bin]
    base_color = np.median(base_px, 0)
    inner_color = np.median(inner_px, 0) if len(inner_px) > 50 else base_color
    blend = gauss(np.where(inner_bin, 1.0, 0.0).astype(np.float32), 2.5)[..., None]
    flat = base_color[None, None, :] * (1 - blend) + inner_color[None, None, :] * blend
    alpha = ref_a
    albedo = np.concatenate([dilate_rgb(flat.astype(np.float32), alpha), alpha[..., None]], -1)
    return finish(name, ref.size, albedo, encode_normal(normal, alpha))


def build_leaf(name="leaf", pillow_pow=0.42, pillow_amp=45.0, midrib_amp=5.0,
               vein_amp=1.5, vein_sigma=4.0, midrib_sigma=7.0):
    ref = Image.open(REFS[name])
    wsize = work_size(ref.size)
    raw_a = load_rgba(REFS[name], wsize)[..., 3]
    albedo_rgb = load_rgba(f"{SP}/{name}_albedo_raw.png", wsize)[..., :3]
    # the reference alpha carries semi-transparent glint strokes cutting the outline;
    # the MCP flat albedo has the full leaf painted solid green — take the silhouette
    # from its green-dominant pixels and keep the reference's soft outer border
    green = albedo_rgb[..., 1] - np.maximum(albedo_rgb[..., 0], albedo_rgb[..., 2])
    solid = fill_holes(np.where(gauss((green > 0.06).astype(np.float32), 2) > 0.5, 1.0, 0.0).astype(np.float32))
    # cap alpha outside the solid outline: the reference glint strokes stick out of the leaf
    ref_a = np.minimum(np.maximum(raw_a, gauss(solid, 1.5)), np.clip(gauss(solid, 2.0)*1.3, 0, 1))
    veins = load_rgba(f"{SP}/{name}_veins_mask.png", wsize)[..., :3].mean(-1)
    veins = np.where(veins > 0.5, 1.0, 0.0).astype(np.float32)

    mask = (ref_a > 0.5).astype(np.float32)
    # relief uses a closed outline: edge notches from the reference glint would crease the dome
    dome_mask = np.where(gauss(np.maximum(mask, solid), 9) > 0.45, 1.0, 0.0).astype(np.float32)
    # heavy blur before the power curve: the raw distance field creases along the shape
    # skeleton (horizontal branches at the widest rows show as seams)
    dt = gauss(chamfer_dt(dome_mask), 10)
    dome = (dt / max(dt.max(), 1.0)) ** pillow_pow * pillow_amp

    # midrib = thick central part of the veins mask: erode-ish via strong blur threshold
    thick = np.where(gauss(veins, 4) > 0.55, 1.0, 0.0).astype(np.float32)
    height = dome + gauss(thick, midrib_sigma) * midrib_amp + gauss(veins, vein_sigma) * vein_amp
    normal = height_to_normal(gauss(height / pillow_amp, 3.0) * pillow_amp, 1.0)

    # synthetic flat albedo: median leaf green + lighter vein lines, kills MCP brush streaks
    mask_bin = mask > 0.5
    veins_soft = np.clip(gauss(veins, 1.2) * 1.6, 0, 1)
    body_px = albedo_rgb[mask_bin & (veins_soft < 0.2)]
    vein_px = albedo_rgb[mask_bin & (veins > 0.5)]
    body_color = np.median(body_px, 0)
    vein_color = np.median(vein_px, 0) if len(vein_px) > 50 else body_color * 1.25
    flat = body_color[None, None, :] * (1 - veins_soft[..., None]) + vein_color[None, None, :] * veins_soft[..., None]
    alpha = ref_a
    albedo = np.concatenate([dilate_rgb(flat.astype(np.float32), alpha), alpha[..., None]], -1)
    return finish(name, ref.size, albedo, encode_normal(normal, alpha))


def finish(name, final_size, albedo, normal):
    a_img = to_img(albedo).resize(final_size, Image.LANCZOS)
    n_img = to_img(normal).resize(final_size, Image.LANCZOS)
    # renormalize after resize
    n = np.asarray(n_img).astype(np.float32) / 255.0
    v = n[..., :3] * 2 - 1
    v /= np.linalg.norm(v, axis=-1, keepdims=True)
    n = np.concatenate([v * 0.5 + 0.5, n[..., 3:4]], -1)
    n_img = to_img(n)
    a_img.save(f"{SP}/{name}_albedo.png")
    n_img.save(f"{SP}/{name}_normal.png")
    print(f"{name}: albedo+normal saved {final_size}")
    return a_img, n_img


# ---------------- shader simulator (must match ChipLit.frag exactly) ----------------

def simulate(albedo_path, normal_path, params, angle_deg=0.0, out=None):
    """Renders the chip like the engine shader would, sprite rotated by angle_deg."""
    a_img = Image.open(albedo_path).convert("RGBA")
    n_img = Image.open(normal_path).convert("RGBA")
    if angle_deg:
        a_img = a_img.rotate(angle_deg, Image.BICUBIC, expand=False)
        n_img = n_img.rotate(angle_deg, Image.BICUBIC, expand=False)
    albedo = np.asarray(a_img).astype(np.float32) / 255.0
    nm = np.asarray(n_img).astype(np.float32) / 255.0

    ln = nm[..., :3] * 2 - 1  # local (sprite-space) normal, from rotated texture
    # sprite rotation: texture rotated by PIL == sprite rotated; vertex normal = sprite X axis
    th = np.deg2rad(angle_deg)
    T = np.array([np.cos(th), np.sin(th)])
    B = np.array([-T[1], T[0]])
    N = np.stack([T[0] * ln[..., 0] + B[0] * ln[..., 1],
                  T[1] * ln[..., 0] + B[1] * ln[..., 1],
                  ln[..., 2]], -1)
    N /= np.maximum(np.linalg.norm(N, axis=-1, keepdims=True), 1e-6)

    ld = np.array(params["lightDir"], np.float32)
    lz = np.sqrt(max(1.0 - ld[0] ** 2 - ld[1] ** 2, 0.0))
    L = np.array([ld[0], ld[1], lz])
    V = np.array([0.0, 0.0, 1.0])
    H = (L + V) / np.linalg.norm(L + V)

    ndl = (N @ L)
    ndv = np.clip(N @ V, 0.0, 1.0)
    diff = np.clip(ndl * 0.5 + 0.5, 0, 1) ** params["diffPow"]
    ndh = np.clip(N @ H, 0, 1)
    spec = ndh ** params["shininess"] * params["specular"] \
         + ndh ** params.get("shininess2", 60.0) * params.get("specular2", 0.0)
    rim = (1.0 - ndv) ** params["rimPow"] * np.clip(0.5 - 0.5 * ndl, 0, 1) * params["rim"]
    fill = np.clip(-ndl, 0, 1) ** 2 * (1.0 - ndv) ** params.get("fillPow", 2.0) * params["fill"]
    nxy = N[..., :2] / np.maximum(np.linalg.norm(N[..., :2], axis=-1, keepdims=True), 1e-6)
    lxy = np.array(params["lightDir"], np.float32)
    lxy = lxy / max(np.linalg.norm(lxy), 1e-6)
    edge = (1.0 - ndv) ** 3 * np.clip(nxy @ lxy, 0, 1) ** params.get("edgePow", 4.0) \
         * params.get("edgeLight", 0.0)

    base = albedo[..., :3]
    shadow = base * base * np.array(params.get("shadowTint", [1, 1, 1]))
    light_amount = np.clip(params["ambient"] + params["diffuse"] * diff, 0, 1)[..., None]
    lit = shadow * (1 - light_amount) + base * light_amount
    lit = lit * (1 - rim[..., None]) + shadow * rim[..., None]
    lit += spec[..., None] * np.array(params.get("specColor", [1, 1, 1]))
    lit += base * fill[..., None]
    lit += edge[..., None] * np.array(params.get("specColor", [1, 1, 1]))

    outa = np.concatenate([np.clip(lit, 0, 1), albedo[..., 3:4]], -1)
    img = to_img(outa)
    if out:
        img.save(out)
    return img


DEFAULT_PARAMS = {
    "lightDir": [-0.10, 0.52], "diffPow": 1.6, "diffuse": 0.85, "ambient": 0.35,
    "specular": 0.55, "shininess": 42.0, "rim": 0.55, "rimPow": 1.7,
    "fill": 0.35, "shadowTint": [0.9, 0.55, 0.85], "specColor": [1.0, 1.0, 1.0],
}

LIGHT_DIR = [-0.18, 0.55]  # single abstract world light shared by every chip

PARAMS = {
    "red": {**DEFAULT_PARAMS, "lightDir": LIGHT_DIR, "diffPow": 2.1, "diffuse": 1.0,
            "ambient": 0.28, "specular": 0.85, "shininess": 30.0, "rim": 0.85, "rimPow": 1.35,
            "fill": 0.25, "shadowTint": [0.85, 0.45, 0.75]},
    "blue": {**DEFAULT_PARAMS, "lightDir": LIGHT_DIR, "diffPow": 2.1, "diffuse": 1.0,
             "ambient": 0.30, "specular": 0.85, "shininess": 30.0, "rim": 0.80, "rimPow": 1.35,
             "fill": 0.30, "shadowTint": [0.55, 0.75, 1.0]},
    "leaf": {**DEFAULT_PARAMS, "lightDir": LIGHT_DIR, "diffPow": 1.5, "diffuse": 0.9,
             "ambient": 0.42, "specular": 0.65, "shininess": 26.0, "rim": 0.50, "rimPow": 1.6,
             "fill": 0.20, "shadowTint": [0.55, 0.9, 0.5]},
}


def calibrate_albedo(name, rounds=2):
    """Scale albedo channels so the simulated render's mean color matches the reference.
    For balls the inner shape and the base are calibrated as separate regions."""
    ref = np.asarray(Image.open(REFS[name]).convert("RGBA")).astype(np.float32) / 255.0
    region = None
    if name in ("red", "blue"):
        wsize = (ref.shape[1], ref.shape[0])
        inner = load_rgba(f"{SP}/{name}_inner_mask.png", wsize)[..., :3].mean(-1)
        inner = clean_mask(inner)
        region = gauss(inner, 2.0)
    for _ in range(rounds):
        sim = simulate(f"{SP}/{name}_albedo.png", f"{SP}/{name}_normal.png", PARAMS[name], 0)
        simn = np.asarray(sim).astype(np.float32) / 255.0
        mask = (ref[..., 3] > 0.5) & (simn[..., 3] > 0.5)
        al = np.asarray(Image.open(f"{SP}/{name}_albedo.png").convert("RGBA")).astype(np.float32) / 255.0
        if region is None:
            scale = ref[..., :3][mask].mean(0) / np.maximum(simn[..., :3][mask].mean(0), 1e-4)
            scale = 1.0 + (scale - 1.0) * 0.85
            al[..., :3] = np.clip(al[..., :3] * scale, 0, 1)
        else:
            scaled = np.zeros_like(al[..., :3])
            weight = np.zeros(al.shape[:2], np.float32)
            for m, w in ((mask & (region > 0.5), region), (mask & (region <= 0.5), 1.0 - region)):
                if m.sum() < 100:
                    continue
                scale = ref[..., :3][m].mean(0) / np.maximum(simn[..., :3][m].mean(0), 1e-4)
                scale = 1.0 + (scale - 1.0) * 0.85
                scaled += np.clip(al[..., :3] * scale, 0, 1) * w[..., None]
                weight += w
            al[..., :3] = scaled / np.maximum(weight, 1e-4)[..., None]
        to_img(al).save(f"{SP}/{name}_albedo.png")
    print(f"{name}: albedo calibrated")


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "all"
    if what in ("all", "build"):
        build_ball("red")
        build_ball("blue")
        build_leaf()
    if what in ("all", "build", "calibrate"):
        for name in ("red", "blue", "leaf"):
            calibrate_albedo(name)
    if what in ("all", "sim"):
        for name in ("red", "blue", "leaf"):
            simulate(f"{SP}/{name}_albedo.png", f"{SP}/{name}_normal.png",
                     PARAMS[name], 0, f"{SP}/{name}_sim0.png")
        print("simulated")
