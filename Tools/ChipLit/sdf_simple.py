#!/usr/bin/env python3
"""Simplified SDF chip model: the reference rim/edge coloring lives entirely in the LUT
(blend = 1), analytic bands are gone. Per-color params shrink to gradients, shadow,
glint, dots; every geometry constant is baked into the shader.

Usage: sdf_simple.py [color...]  — converts seeds, rebakes LUTs with the unified light,
refits the remaining scalars/colors, prints gate metrics, writes {name}_sdf_simple.json.
"""
import numpy as np
from PIL import Image
import os, sys, json

SP = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SP)
from sdf import (TEX, DF_RANGE, LUT_W, LUT_RIM_RANGE, LUT_ICON_RANGE,
                 build_rim_lut, sample_lut, smoothstep, load_df)
from refine import pixel_error, load, REFS
from details import shadow_score, crescent_score, glint_dots_score, \
    shadow_depth_score, sharpness_score

UNIFIED_LIGHT = np.array([-0.3292, 0.9443], np.float32)
SHADOW_DIR = np.array([0.7071, -0.7071], np.float32)
SHADOW_OFFSET = 30.0
SHADOW_SOFT = 3.0
ICON_AA = 2.5
COLORS_ALL = ["red", "blue", "green", "orange", "violet", "yellow"]


def simplify_seed(name):
    p = json.load(open(f"{SP}/{name}_sdf_params.json"))
    keep = ["gradSpread", "gradPow", "baseTop", "baseBottom",
            "iconGradSpread", "iconGradPow", "iconTop", "iconBottom",
            "shadowColor", "shadowStrength",
            "glintDir", "glintRad", "glintRx", "glintRy", "glintPow", "glintStrength",
            "glintColor", "dots", "dotColor"]
    return {k: p[k] for k in keep if k in p}


def render_simple(name, p, angle_deg=0.0, size=None):
    """Numpy twin of the simplified chip_sdf shader (must match it 1:1)."""
    dfm = load_df(f"{SP}/{name}_df.png", size)
    n = dfm.shape[0]
    scale = n/TEX
    base_d = (dfm[..., 0] - 0.5)*DF_RANGE*2*scale
    icon_d = (dfm[..., 1] - 0.5)*DF_RANGE*2*scale
    meta = json.load(open(f"{SP}/{name}_df.json"))
    a_off = meta.get("alphaOff", 0.0)*scale
    a_soft = max(meta.get("alphaSoft", 1.5)*scale, 0.4)
    alpha = smoothstep(a_off - a_soft, a_off + a_soft, base_d)
    gdir = dfm[..., 2:4]*2.0 - 1.0
    gdir = gdir/np.maximum(np.linalg.norm(gdir, axis=-1, keepdims=True), 1e-6)

    th = np.deg2rad(angle_deg)

    def w2s(v):
        c, s = np.cos(-th), np.sin(-th)
        return np.array([v[0]*c - v[1]*s, v[0]*s + v[1]*c], np.float32)

    Ls = w2s(UNIFIED_LIGHT)
    cy, cx = meta["cy"]*n, meta["cx"]*n
    Rpx = meta["R"]*n
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    rel = np.stack([(xx - cx), -(yy - cy)], -1)
    rad = rel/np.maximum(np.linalg.norm(rel, axis=-1, keepdims=True), 1e-6)

    # base gradient along the light
    t = np.clip(0.5 + (rel @ Ls)/(Rpx*p["gradSpread"]), 0, 1) ** p["gradPow"]
    top = np.array(p["baseTop"]); bot = np.array(p["baseBottom"])
    color = bot[None, None, :]*(1 - t[..., None]) + top[None, None, :]*t[..., None]

    lut = np.asarray(Image.open(f"{SP}/{name}_lut.png")).astype(np.float32)/255.0
    lut = lut[..., :3]
    if lut.shape[1] != LUT_W:
        lut = lut[:, 1:1 + LUT_W]

    # rim: pure LUT (the baked reference ring), fading into the gradient interior
    ang_b = np.arctan2(rad[..., 0]*Ls[1] - rad[..., 1]*Ls[0], rad @ Ls)
    dn = base_d/(LUT_RIM_RANGE*scale)
    lutc = sample_lut(lut, 1, 9, ang_b, dn)
    fade = (1.0 - smoothstep(0.75, 1.0, dn))*smoothstep(-3.0, 1.0, base_d)*p.get("rimBlend", 1.0)
    color = color*(1 - fade[..., None]) + lutc*fade[..., None]

    # icon drop shadow: silhouette offset along the world shadow dir
    off = w2s(SHADOW_DIR)*SHADOW_OFFSET*scale
    oy, ox = off[1], -off[0]
    sy = np.clip(yy + oy, 0, n - 1).astype(int)
    sx = np.clip(xx + ox, 0, n - 1).astype(int)
    icon_off_d = icon_d[sy, sx]
    icon_mask = smoothstep(-ICON_AA*scale, ICON_AA*scale, icon_d)
    sh = smoothstep(-SHADOW_SOFT*scale, SHADOW_SOFT*scale, icon_off_d) \
        * p["shadowStrength"]*(1 - icon_mask)
    color = color*(1 - sh[..., None]) + np.array(p["shadowColor"])[None, None, :]*sh[..., None]

    # elliptic glint, world-anchored
    Gd = np.array(p["glintDir"], np.float32)
    Gd = Gd/max(np.linalg.norm(Gd), 1e-6)
    Gs = w2s(Gd)
    gpos = np.array([cx, cy]) + np.array([Gs[0], -Gs[1]])*Rpx*p["glintRad"]
    gd = np.hypot((xx - gpos[0])/(p["glintRx"]*scale), (yy - gpos[1])/(p["glintRy"]*scale))
    g = np.clip(1 - gd, 0, 1) ** p["glintPow"] * p["glintStrength"]
    color = color + (np.array(p["glintColor"])[None, None, :] - color)*g[..., None]

    # icon: gradient + pure LUT edges
    t2 = np.clip(0.5 + (rel @ Ls)/(Rpx*p["iconGradSpread"]), 0, 1) ** p["iconGradPow"]
    itop = np.array(p["iconTop"]); ibot = np.array(p["iconBottom"])
    icol = ibot[None, None, :]*(1 - t2[..., None]) + itop[None, None, :]*t2[..., None]

    ang_i = np.arctan2(gdir[..., 0]*Ls[1] - gdir[..., 1]*Ls[0], gdir @ Ls)
    dni = icon_d/(LUT_ICON_RANGE*scale)
    lutci = sample_lut(lut, 14, 9, ang_i, dni)
    fadei = (1.0 - smoothstep(0.7, 1.0, dni))*smoothstep(-1.0, 1.5, icon_d)*p.get("iconBlend", 1.0)
    icol = icol*(1 - fadei[..., None]) + lutci*fadei[..., None]

    color = color*(1 - icon_mask[..., None]) + icol*icon_mask[..., None]

    # glint dots
    dot_color = np.array(p.get("dotColor", [1.0, 1.0, 1.0]))
    for dot in p.get("dots", []):
        ddir = np.array(dot[0:2], np.float32)
        ddir = ddir/max(np.linalg.norm(ddir), 1e-6)
        Ds = w2s(ddir)
        dist, radius, strength = dot[2]*scale, dot[3]*scale, dot[4]
        dpos = np.array([cx, cy]) + np.array([Ds[0], -Ds[1]])*dist
        dd = np.hypot(xx - dpos[0], yy - dpos[1])/max(radius, 1e-3)
        g = (1.0 - smoothstep(0.0, 1.0, dd)) ** 1.5 * strength
        color = color*(1 - g[..., None]) + dot_color[None, None, :]*g[..., None]

    return np.concatenate([np.clip(color, 0, 1), alpha[..., None]], -1)


SCALARS = {
    "gradSpread": (0.6, 4.0), "gradPow": (0.5, 2.5),
    "iconGradSpread": (0.6, 4.0), "iconGradPow": (0.5, 2.5),
    "shadowStrength": (0, 1.3),
    "glintRad": (0.3, 0.95), "glintRx": (4, 60), "glintRy": (4, 40),
    "glintPow": (1, 5), "glintStrength": (0, 1.3),
}
FIT_COLORS = ["baseTop", "baseBottom", "iconTop", "iconBottom", "shadowColor", "glintColor"]
COLOR_R = 0.18


def detail_penalty(name, r, ref):
    sh, _, _ = shadow_score(name, r, ref, tuple(UNIFIED_LIGHT))
    cr, _, _ = crescent_score(name, r, ref)
    gd, _ = glint_dots_score(name, r, ref)
    sd, _, _ = shadow_depth_score(name, r, ref)
    sharp = sharpness_score(name, r, ref)
    return 0.04*(100 - sh) + 0.05*(100 - cr) + 0.04*(100 - gd) + 0.05*(100 - sd) \
        + 0.025*(100 - sharp["rim"][0]) + 0.025*(100 - sharp["icon"][0])


def fit(name, seed, iters=700, rng_seed=5):
    ref = load(REFS[name])
    n = ref.shape[0]
    rng = np.random.default_rng(rng_seed)
    seeds_c = {k: np.array(seed[k]) for k in FIT_COLORS}

    def err(pp):
        r = render_simple(name, pp, 0, size=n)
        e_mean, e_bad, _, _ = pixel_error(r, ref)
        return e_mean + 2.0*e_bad + detail_penalty(name, r, ref)

    def perturb(pp, scale):
        q = json.loads(json.dumps(pp))
        for k in SCALARS:
            if rng.random() < 0.35:
                lo, hi = SCALARS[k]
                q[k] = float(np.clip(q[k] + rng.normal(0, scale)*(hi - lo)*0.5, lo, hi))
        for k in FIT_COLORS:
            if rng.random() < 0.4:
                v = np.array(q[k]) + rng.normal(0, scale*COLOR_R, 3)
                q[k] = list(np.clip(np.clip(v, seeds_c[k] - COLOR_R, seeds_c[k] + COLOR_R), 0, 1))
        if rng.random() < 0.3:
            v = np.array(q["glintDir"]) + rng.normal(0, scale*0.3, 2)
            q["glintDir"] = list(v/max(np.linalg.norm(v), 1e-6))
        for dot in q.get("dots", []):
            if rng.random() < 0.3:
                dot[2] = float(np.clip(dot[2] + rng.normal(0, scale*10), 10, 500))
                dot[3] = float(np.clip(dot[3] + rng.normal(0, scale*8), 3, 34))
                dot[4] = float(np.clip(dot[4] + rng.normal(0, scale*0.5), 0.0, 1.2))
        return q

    best, best_e = seed, err(seed)
    scale = 0.25
    for i in range(iters):
        q = perturb(best, scale)
        e = err(q)
        if e < best_e:
            best, best_e = q, e
        if i % 120 == 119:
            scale *= 0.75
    return best, best_e


def ring_metric(name, p, n):
    """0 vs 90 deg consistency on the ball body (icon zone excluded generously)."""
    r0 = render_simple(name, p, 0, size=n)
    r90 = render_simple(name, p, 90, size=n)
    r90b = np.rot90(r90, 1)  # rotate the sprite back: world features must line up
    df = load_df(f"{SP}/{name}_df.png", n)
    scale = n/TEX
    base_d = (df[..., 0] - 0.5)*DF_RANGE*2*scale
    icon_d = (df[..., 1] - 0.5)*DF_RANGE*2*scale
    icon_zone = icon_d > -14*scale
    zone = (base_d > 2) & ~icon_zone & ~np.rot90(icon_zone, 1) & ~np.rot90(icon_zone, -1)
    d = np.linalg.norm(r0[..., :3] - r90b[..., :3], axis=-1)/np.sqrt(3)
    return d[zone].mean()*100


def run(names):
    report = {}
    for name in names:
        print(f"== {name}")
        build_rim_lut(name, UNIFIED_LIGHT)
        seed = simplify_seed(name)
        ref = load(REFS[name])
        n = ref.shape[0]
        r = render_simple(name, seed, 0, size=n)
        e0 = pixel_error(r, ref)[:2]
        p, e = fit(name, seed)
        r = render_simple(name, p, 0, size=n)
        e_mean, e_bad, _, _ = pixel_error(r, ref)
        ring = ring_metric(name, p, n)
        det = detail_penalty(name, r, ref)
        print(f"  seed {e0[0]:.2f}/{e0[1]:.2f} -> fit {e_mean:.2f}%/{e_bad:.2f}% "
              f"ring={ring:.2f}% detpen={det:.1f}")
        json.dump(p, open(f"{SP}/{name}_sdf_simple.json", "w"), indent=1, default=float)
        report[name] = dict(mean=e_mean, bad=e_bad, ring=ring)
        Image.fromarray((np.clip(r, 0, 1)*255).astype(np.uint8)).save(f"{SP}/{name}_simple_render.png")
    json.dump(report, open(f"{SP}/simple_report.json", "w"), indent=1)
    return report


if __name__ == "__main__":
    run(sys.argv[1:] or COLORS_ALL)
