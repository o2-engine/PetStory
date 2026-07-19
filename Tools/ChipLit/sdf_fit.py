#!/usr/bin/env python3
"""Staged fit of the SDF chip model: base-region params, icon-region params, joint.
Colors are measured seeds refined within a small radius; geometry fitted freely."""
import numpy as np
from PIL import Image
import os, sys, json

SP = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SP)
from sdf import render_sdf, TEX, DF_RANGE
from sdf_seed import make_params
from refine import pixel_error, load, composite, REFS

SCALARS = {
    "gradSpread": (0.6, 4.0), "gradPow": (0.5, 2.5),
    "darkPos": (2, 30), "darkWidth": (4, 40), "darkSoft": (2, 30), "darkStrength": (0, 1),
    "litPos": (2, 30), "litWidth": (4, 40), "litSoft": (2, 30), "litStrength": (0, 1),
    "azEdge0": (-0.5, 0.6), "azEdge1": (0.2, 1.0),
    "bounceWidePos": (2, 40), "bounceWideWidth": (4, 50), "bounceWideSoft": (2, 30),
    "bounceWideStrength": (0, 1),
    "bounceLinePos": (1, 20), "bounceLineWidth": (2, 16), "bounceLineSoft": (1, 12),
    "bounceLineStrength": (0, 1),
    "azB0": (-0.3, 0.7), "azB1": (0.3, 1.0),
    "shadowOffset": (2, 26), "shadowSoft": (1.5, 14), "shadowStrength": (0, 1.2),
    "glintRad": (0.3, 0.95), "glintRx": (6, 60), "glintRy": (4, 40), "glintPow": (1, 5),
    "glintStrength": (0, 1),
    "iconGradSpread": (0.6, 4.0), "iconGradPow": (0.5, 2.5),
    "iconLitPos": (1, 25), "iconLitWidth": (2, 30), "iconLitSoft": (1, 20), "iconLitStrength": (0, 1),
    "iconShadePos": (1, 25), "iconShadeWidth": (2, 30), "iconShadeSoft": (1, 20), "iconShadeStrength": (0, 1),
    "iconBouncePos": (1, 20), "iconBounceWidth": (2, 20), "iconBounceSoft": (1, 15), "iconBounceStrength": (0, 1),
    "iazEdge0": (-0.5, 0.6), "iazEdge1": (0.2, 1.0),
    "iconGlintPos": (1, 20), "iconGlintWidth": (1, 14), "iconGlintSoft": (0.5, 10), "iconGlintStrength": (0, 1),
    "iglAz0": (-0.3, 0.7), "iglAz1": (0.3, 1.0),
    "shadowGrow": (0, 20),
}
DIRS = {"lightDir": 0.35, "bounceDir": 0.35}   # max perturbation of direction components
COLORS = ["baseTop", "baseBottom", "darkColor", "litColor", "bounceColor", "bounceLineColor",
          "shadowColor", "glintColor", "iconTop", "iconBottom", "iconLitColor",
          "iconShadeColor", "iconBounceColor", "iconGlintColor"]
COLOR_R = 0.18  # allowed drift around the measured color

BASE_KEYS = ["gradSpread", "gradPow", "darkPos", "darkWidth", "darkSoft", "darkStrength",
             "litPos", "litWidth", "litSoft", "litStrength", "azEdge0", "azEdge1",
             "bounceWidePos", "bounceWideWidth", "bounceWideSoft", "bounceWideStrength",
             "bounceLinePos", "bounceLineWidth", "bounceLineSoft", "bounceLineStrength",
             "azB0", "azB1", "shadowOffset", "shadowSoft", "shadowStrength", "shadowGrow",
             "glintRad", "glintRx", "glintRy", "glintPow", "glintStrength"]
BASE_COLORS = ["baseTop", "baseBottom", "darkColor", "litColor", "bounceColor",
               "bounceLineColor", "shadowColor", "glintColor"]
ICON_KEYS = ["iconGradSpread", "iconGradPow", "iconLitPos", "iconLitWidth", "iconLitSoft",
             "iconLitStrength", "iconShadePos", "iconShadeWidth", "iconShadeSoft",
             "iconShadeStrength", "iconBouncePos", "iconBounceWidth", "iconBounceSoft",
             "iconBounceStrength", "iazEdge0", "iazEdge1", "iconGlintPos", "iconGlintWidth",
             "iconGlintSoft", "iconGlintStrength", "iglAz0", "iglAz1"]
ICON_COLORS = ["iconTop", "iconBottom", "iconLitColor", "iconShadeColor", "iconBounceColor",
               "iconGlintColor"]


def regions(name, n):
    df = np.asarray(Image.open(f"{SP}/{name}_df.png").resize((n, n), Image.LANCZOS)).astype(np.float32)/255.0
    scale = n/TEX
    icon_d = (df[..., 1] - 0.5)*DF_RANGE*2*scale
    icon = icon_d > -6  # icon + its shadow rim
    return ~icon, icon


def fit(name, seed, rounds=(500, 500, 400), rng_seed=3):
    ref = load(REFS[name])
    n = ref.shape[0]
    base_reg, icon_reg = regions(name, n)
    rng = np.random.default_rng(rng_seed)
    p = json.loads(json.dumps(seed))
    seeds_c = {k: np.array(seed[k]) for k in COLORS}
    seeds_d = {k: np.array(seed[k]) for k in DIRS}

    from details import shadow_score, crescent_score, glint_dots_score, \
        shadow_depth_score, sharpness_score

    def render(pp):
        img = render_sdf(f"{SP}/{name}_df.png", pp, 0, size=n)
        return img

    def detail_penalty(pp, r):
        sh, _, _ = shadow_score(name, r, ref, pp["lightDir"])
        cr, _, _ = crescent_score(name, r, ref)
        gd, _ = glint_dots_score(name, r, ref)
        sd, _, _ = shadow_depth_score(name, r, ref)
        sharp = sharpness_score(name, r, ref)
        return 0.04*(100 - sh) + 0.05*(100 - cr) + 0.04*(100 - gd) + 0.05*(100 - sd) \
             + 0.025*(100 - sharp["rim"][0]) + 0.025*(100 - sharp["icon"][0])

    def err(pp, mask=None):
        r = render(pp)
        e_mean, e_bad, dmap, m = pixel_error(r, ref)
        det = detail_penalty(pp, r)
        if mask is not None:
            mm = m & mask
            e_zone = dmap[mm].mean()*100 if mm.sum() > 50 else 0.0
            bad_zone = (dmap[mm] > 0.2).mean()*100 if mm.sum() > 50 else 0.0
            return e_zone + 2.0*bad_zone + 0.25*(e_mean + 2.0*e_bad) + det
        return e_mean + 2.0*e_bad + det

    def perturb(pp, keys, colors, scale):
        q = json.loads(json.dumps(pp))
        for k in keys:
            lo, hi = SCALARS[k]
            q[k] = float(np.clip(q[k] + rng.normal(0, scale)*(hi - lo)*0.5, lo, hi))
        for k in colors:
            if rng.random() < 0.4:
                v = np.array(q[k]) + rng.normal(0, scale*COLOR_R, 3)
                q[k] = list(np.clip(np.clip(v, seeds_c[k] - COLOR_R, seeds_c[k] + COLOR_R), 0, 1))
        for k in DIRS:
            if rng.random() < 0.3:
                v = np.array(q[k]) + rng.normal(0, scale*DIRS[k], 2)
                v = np.clip(v, seeds_d[k] - DIRS[k], seeds_d[k] + DIRS[k])
                q[k] = list(v/max(np.linalg.norm(v), 1e-6))
        for dot in q.get("dots", []):
            if rng.random() < 0.3:
                dot[2] = float(np.clip(dot[2] + rng.normal(0, scale*10), 10, 500))
                dot[3] = float(np.clip(dot[3] + rng.normal(0, scale*8), 3, 34))
                dot[4] = float(np.clip(dot[4] + rng.normal(0, scale*0.5), 0.15, 1.2))
        return q

    def stage(pp, keys, colors, mask, iters, label):
        best, best_e = pp, err(pp, mask)
        scale = 0.25
        for i in range(iters):
            q = perturb(best, [k for k in keys if rng.random() < 0.3] or [rng.choice(keys)],
                        colors, scale)
            e = err(q, mask)
            if e < best_e:
                best, best_e = q, e
            if i % 100 == 99:
                scale *= 0.75
        print(f"  {label}: {best_e:.2f}")
        return best

    p = stage(p, BASE_KEYS, BASE_COLORS, base_reg, rounds[0], "base")
    p = stage(p, ICON_KEYS, ICON_COLORS, icon_reg, rounds[1], "icon")
    p = stage(p, BASE_KEYS + ICON_KEYS, BASE_COLORS + ICON_COLORS, None, rounds[2], "joint")

    r = render(p)
    e_mean, e_bad, _, _ = pixel_error(r, ref)
    return p, e_mean, e_bad


if __name__ == "__main__":
    for name in (sys.argv[1],) if len(sys.argv) > 1 else ("blue", "red"):
        seed = make_params(name)
        p, e_mean, e_bad = fit(name, seed)
        json.dump(p, open(f"{SP}/{name}_sdf_params.json", "w"), indent=1, default=float)
        print(f"{name}: fitted mean={e_mean:.2f}% bad={e_bad:.2f}%")
