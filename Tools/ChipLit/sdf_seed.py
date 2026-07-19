#!/usr/bin/env python3
"""Seeds sdf-model params by measuring the reference: region colors are sampled
directly, geometry starts from hand priors. Prints the first error numbers."""
import numpy as np
from PIL import Image
import os, sys, json

SP = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SP)
from sdf import render_sdf, TEX, DF_RANGE
from refine import pixel_error, load, composite, REFS


def sample_colors(name):
    ref = load(REFS[name])
    n = ref.shape[0]
    df = np.asarray(Image.open(f"{SP}/{name}_df.png").resize((n, n), Image.LANCZOS)).astype(np.float32)/255.0
    scale = n/TEX
    base_d = (df[..., 0] - 0.5)*DF_RANGE*2*scale
    icon_d = (df[..., 1] - 0.5)*DF_RANGE*2*scale
    meta = json.load(open(f"{SP}/{name}_df.json"))
    cy, cx = meta["cy"]*n, meta["cx"]*n
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    rel = np.stack([(xx - cx), -(yy - cy)], -1)
    L = np.array([-0.25, 0.97]); L /= np.linalg.norm(L)
    rad = rel/np.maximum(np.linalg.norm(rel, axis=-1, keepdims=True), 1e-6)
    az = rad @ L
    tl = rel @ L

    body = (base_d > 12) & (icon_d < -6)
    icon = icon_d > 4
    c = lambda m: [float(x) for x in np.median(ref[..., :3][m], 0)] if m.sum() > 20 else [0.5, 0.5, 0.5]

    lum = ref[..., :3].mean(-1)
    glint_zone = body & (tl > 0.3*n/2)
    gbest = np.unravel_index(np.argmax(np.where(glint_zone, lum, -1)), lum.shape)

    seeds = dict(
        baseTop=c(body & (tl > 0.15*n/2)), baseBottom=c(body & (tl < -0.15*n/2)),
        darkColor=c((base_d > 2) & (base_d < 10) & (az < -0.4) & ~icon),
        litColor=c((base_d > 2) & (base_d < 10) & (az > 0.4) & ~icon),
        bounceColor=c((base_d > 2) & (base_d < 14) & ((rad @ np.array([0.55, -0.83])) > 0.5) & ~icon),
        iconTop=c(icon & (tl > 0.1*n/2)), iconBottom=c(icon & (tl < -0.1*n/2)),
        glintColor=[float(x) for x in ref[..., :3][gbest[0], gbest[1]]],
        glintPos=(float(gbest[1]), float(gbest[0])),
        glintDir=[float(x) for x in
                  np.array([gbest[1] - cx, -(gbest[0] - cy)]) /
                  max(np.hypot(gbest[1] - cx, gbest[0] - cy), 1e-6)],
    )
    return seeds


def measure_dots(name, k=4):
    """Bright peaks of the reference become round glint dots: world direction + distance
    from the chip center, radius and strength scaled from the peak prominence."""
    from pipeline import gauss
    from sdf import TEX
    ref = load(REFS[name])
    n = ref.shape[0]
    l = 0.2126*ref[..., 0] + 0.7152*ref[..., 1] + 0.0722*ref[..., 2]
    prom = l - gauss(l, 6)
    meta = json.load(open(f"{SP}/{name}_df.json"))
    cy, cx = meta["cy"]*n, meta["cx"]*n
    mask = ref[..., 3] > 0.6
    work = np.where(mask, prom, -1)
    dots, color = [], None
    scale = TEX/n
    for _ in range(k):
        idx = np.unravel_index(np.argmax(work), work.shape)
        v = work[idx]
        if v < 0.05:
            break
        y, x = idx
        d = np.array([x - cx, -(y - cy)], np.float32)
        dist = float(np.linalg.norm(d))
        ddir = d/max(dist, 1e-6)
        dots.append([float(ddir[0]), float(ddir[1]), dist*scale,
                     float(np.clip(4.0 + v*30.0, 5, 14))*scale, float(np.clip(v*4.0, 0.4, 1.0))])
        if color is None:
            color = [float(c) for c in ref[y, x, :3]]
        work[max(0, y-9):y+10, max(0, x-9):x+10] = -1
    return dots, (color or [1.0, 1.0, 1.0])


def make_params(name):
    s = sample_colors(name)
    dots, dot_color = measure_dots(name)
    # texture-space px units (TEX=420)
    p = dict(
        lightDir=[-0.25, 0.97], bounceDir=[0.55, -0.83],
        gradSpread=1.6, gradPow=1.0, baseTop=s["baseTop"], baseBottom=s["baseBottom"],
        darkPos=8.0, darkWidth=16.0, darkSoft=12.0, darkStrength=0.85, darkColor=s["darkColor"],
        litPos=8.0, litWidth=14.0, litSoft=10.0, litStrength=0.8, litColor=s["litColor"],
        azEdge0=0.0, azEdge1=0.7,
        bounceWidePos=14.0, bounceWideWidth=22.0, bounceWideSoft=14.0, bounceWideStrength=0.5,
        bounceColor=s["bounceColor"],
        bounceLinePos=5.0, bounceLineWidth=6.0, bounceLineSoft=4.0, bounceLineStrength=0.7,
        bounceLineColor=list(np.clip(np.array(s["bounceColor"])*1.25, 0, 1)),
        azB0=0.2, azB1=0.8,
        shadowOffset=10.0, shadowSoft=10.0, shadowStrength=0.45,
        shadowColor=list(np.clip(np.array(s["baseBottom"])*0.55, 0, 1)),
        glintDir=s["glintDir"],
        glintRad=0.85, glintRx=16.0, glintRy=9.0, glintPow=2.0, glintStrength=1.0,
        glintColor=s["glintColor"],
        iconAA=2.5, iconGradSpread=1.8, iconGradPow=1.0, iconTop=s["iconTop"], iconBottom=s["iconBottom"],
        iconLitPos=7.0, iconLitWidth=12.0, iconLitSoft=9.0, iconLitStrength=0.6,
        iconLitColor=list(np.clip(np.array(s["iconTop"])*1.2, 0, 1)),
        iconShadePos=7.0, iconShadeWidth=12.0, iconShadeSoft=9.0, iconShadeStrength=0.5,
        iconShadeColor=list(np.clip(np.array(s["iconBottom"])*0.7, 0, 1)),
        iconBouncePos=5.0, iconBounceWidth=8.0, iconBounceSoft=6.0, iconBounceStrength=0.4,
        iconBounceColor=list(np.clip(np.array(s["iconTop"])*1.1, 0, 1)),
        iazEdge0=0.0, iazEdge1=0.7,
        iconGlintPos=6.0, iconGlintWidth=6.0, iconGlintSoft=3.0, iconGlintStrength=0.85,
        iconGlintColor=[1.0, 1.0, 1.0],
        iglAz0=0.15, iglAz1=0.75,
        shadowGrow=6.0,
        dots=dots, dotColor=dot_color,
    )
    return p


def evaluate(name, params, save_prefix=None):
    ref = load(REFS[name])
    n = ref.shape[0]
    big = render_sdf(f"{SP}/{name}_df.png", params, 0, size=TEX)
    img = Image.fromarray((np.clip(big, 0, 1)*255).astype(np.uint8)).resize((n, n), Image.LANCZOS)
    r = np.asarray(img).astype(np.float32)/255.0
    e_mean, e_bad, dmap, mask = pixel_error(r, ref)
    if save_prefix:
        heat = np.zeros((n, n, 3), np.float32)+0.1
        hm = np.clip(dmap/0.3, 0, 1)
        heat[mask] = np.stack([hm[mask], 0.15+0.2*(1-hm[mask]), 1-hm[mask]], -1)
        row = np.concatenate([composite(ref), composite(r), heat], 1)
        Image.fromarray((np.clip(row, 0, 1)*255).astype(np.uint8)).resize((n*6, n*2), Image.NEAREST)\
            .save(f"{SP}/{save_prefix}.png")
    return e_mean, e_bad


if __name__ == "__main__":
    for name in ("blue", "red"):
        p = make_params(name)
        json.dump(p, open(f"{SP}/{name}_sdf_params.json", "w"), indent=1, default=float)
        e_mean, e_bad = evaluate(name, p, f"{name}_sdf_seed")
        print(f"{name}: seed mean={e_mean:.2f}% bad={e_bad:.2f}%")
