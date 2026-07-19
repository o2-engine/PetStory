#!/usr/bin/env python3
"""Feature-level metrics: the mean-error gate misses hand-drawn details, so the
icon drop shadow, the rim crescents and the round glint dots get their own scores.

All scores are 0-100, ref-vs-ref = 100. Renders and refs are RGBA at ref size,
composited over the shared background before comparison.
"""
import numpy as np
from PIL import Image
import os, sys, json

SP = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SP)
from sdf import TEX, DF_RANGE
from refine import composite, load, REFS
from pipeline import gauss


def lum(a):
    return 0.2126*a[..., 0] + 0.7152*a[..., 1] + 0.0722*a[..., 2]


def chip_fields(name, n):
    df = np.asarray(Image.open(f"{SP}/{name}_df.png").resize((n, n), Image.LANCZOS)).astype(np.float32)/255.0
    scale = n/TEX
    base_d = (df[..., 0] - 0.5)*DF_RANGE*2*scale
    icon_d = (df[..., 1] - 0.5)*DF_RANGE*2*scale
    return base_d, icon_d


def shadow_score(name, render, ref, light=( -0.25, 0.97)):
    """Localized error in the icon drop-shadow zone: the band around the icon on the
    side opposite the light."""
    n = ref.shape[0]
    base_d, icon_d = chip_fields(name, n)
    ys, xs = np.nonzero(icon_d > 0)
    cy, cx = ys.mean(), xs.mean()
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    away = np.stack([xx - cx, -(yy - cy)], -1)
    away = away/np.maximum(np.linalg.norm(away, axis=-1, keepdims=True), 1e-6)
    L = np.array(light)/np.linalg.norm(light)
    zone = (icon_d < -2) & (icon_d > -26) & (base_d > 4) & ((away @ (-L)) > 0.0)
    err = np.abs(composite(render) - composite(ref)).mean(-1)[zone].mean()
    # amplitude diagnostics: how much darkening each image has in the zone vs its body
    body = (base_d > 10) & (icon_d < -30)
    amp = lambda img: float(lum(img[..., :3])[body].mean() - lum(img[..., :3])[zone].mean())
    return float(100*max(0, 1 - err/0.10)), amp(ref), amp(render)


def crescent_score(name, render, ref):
    """Rim band brightness profiles per azimuth sector: catches crescents that are
    too wide, too strong or missing."""
    n = ref.shape[0]
    base_d, icon_d = chip_fields(name, n)
    ys, xs = np.nonzero(base_d > 0)
    cy, cx = ys.mean(), xs.mean()
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    ang = np.arctan2(-(yy - cy), xx - cx)
    ring = (base_d > 0) & (base_d < 30) & (icon_d < -2)
    lr, lf = lum(composite(render)), lum(composite(ref))
    diffs = []
    for s in range(16):
        a0 = -np.pi + s*(2*np.pi/16)
        sector = ring & (ang >= a0) & (ang < a0 + 2*np.pi/16)
        prof_r, prof_f = [], []
        for b in range(6):
            sel = sector & (base_d >= b*5) & (base_d < (b + 1)*5)
            if sel.sum() > 6:
                prof_r.append(lr[sel].mean())
                prof_f.append(lf[sel].mean())
        if len(prof_r) >= 4:
            diffs.append(np.sqrt(np.mean((np.array(prof_r) - np.array(prof_f))**2)))
    worst = max(diffs) if diffs else 0.0
    mean = float(np.mean(diffs)) if diffs else 0.0
    return float(100*max(0, 1 - (0.6*worst + 0.4*mean)/0.10)), worst, mean


def glint_dots_score(name, render, ref, k=3):
    """Round glint dots: local luminance maxima of the reference must exist in the
    render at the same spots with comparable prominence."""
    n = ref.shape[0]
    base_d, _ = chip_fields(name, n)
    mask = base_d > 3
    def prominence(img):
        l = lum(composite(img))
        return l - gauss(l, 6)
    pr_ref = prominence(ref)
    pr_ren = prominence(render)
    work = np.where(mask, pr_ref, -1)
    peaks = []
    for _ in range(k):
        idx = np.unravel_index(np.argmax(work), work.shape)
        v = work[idx]
        if v < 0.04:
            break
        peaks.append((idx, float(v)))
        y, x = idx
        work[max(0, y-9):y+10, max(0, x-9):x+10] = -1
    if not peaks:
        return 100.0, []
    scores, det = [], []
    for (y, x), v in peaks:
        w = pr_ren[max(0, y-5):y+6, max(0, x-5):x+6]
        got = float(w.max()) if w.size else 0.0
        s = min(got/v, 1.2)
        scores.append(min(s, 1.0))
        det.append(dict(y=int(y), x=int(x), ref=round(v, 3), got=round(got, 3)))
    return float(100*np.mean(scores)), det


def shadow_depth_score(name, render, ref):
    """Depth of the icon drop shadow (darkest dip below the body level) must match."""
    n = ref.shape[0]
    base_d, icon_d = chip_fields(name, n)
    zone = (icon_d < -2) & (icon_d > -24) & (base_d > 4)
    body = (base_d > 10) & (icon_d < -30)
    lr, lf = lum(composite(render)), lum(composite(ref))
    depth = lambda l: max(float(l[body].mean() - np.percentile(l[zone], 3)), 1e-4)
    ratio = depth(lr)/depth(lf)
    return float(100*max(0.0, 1.0 - abs(np.log(ratio))/np.log(2.2))), round(depth(lf), 3), round(depth(lr), 3)


def sharpness_score(name, render, ref, scale=1.4):
    """Light-feature sharpness at display scale: the reference texture is shown
    magnified (soft), so the render's edges must not be crisper than that."""
    n = ref.shape[0]
    m = int(n*scale)
    up = lambda img: np.asarray(Image.fromarray((np.clip(img, 0, 1)*255).astype(np.uint8))
                                .resize((m, m), Image.LANCZOS)).astype(np.float32)/255.0
    rr, ff = up(composite(render)), up(composite(ref))
    base_d, icon_d = chip_fields(name, m)
    lr, lf = lum(rr), lum(ff)
    gmag = lambda l: np.hypot(*np.gradient(l))
    gr, gf = gmag(lr), gmag(lf)
    out = {}
    for zn, zm in (("rim", (base_d > 2) & (base_d < 30*scale) & (icon_d < -4)),
                   ("icon", np.abs(icon_d) < 10*scale)):
        ratio = float(np.percentile(gr[zm], 95)/max(np.percentile(gf[zm], 95), 1e-5))
        # one-sided: sharper than the displayed reference is penalized, softer is fine
        # (per contributor feedback the hand-drawn look must not become harsher)
        over = max(np.log(ratio) - np.log(1.05), 0.0)
        out[zn] = (float(100*max(0.0, 1.0 - over/np.log(1.9))), round(ratio, 2))
    return out


def detail_report(name, render, ref, light=(-0.25, 0.97)):
    sh, amp_ref, amp_ren = shadow_score(name, render, ref, light)
    cr, worst, mean = crescent_score(name, render, ref)
    gd, det = glint_dots_score(name, render, ref)
    sd, dref, dren = shadow_depth_score(name, render, ref)
    sharp = sharpness_score(name, render, ref)
    return dict(shadow=round(sh, 1), shadow_amp_ref=round(amp_ref, 3), shadow_amp_render=round(amp_ren, 3),
                shadow_depth=round(sd, 1), depth_ref=dref, depth_render=dren,
                crescent=round(cr, 1), crescent_worst=round(worst, 3),
                dots=round(gd, 1), dots_detail=det,
                sharp_rim=round(sharp["rim"][0], 1), sharp_rim_ratio=sharp["rim"][1],
                sharp_icon=round(sharp["icon"][0], 1), sharp_icon_ratio=sharp["icon"][1])


if __name__ == "__main__":
    # validate on current sim renders and on ref-vs-ref
    from sdf import render_sdf
    for name in ("red", "blue"):
        ref = load(REFS[name])
        n = ref.shape[0]
        p = json.load(open(f"{SP}/{name}_sdf_params.json"))
        r = render_sdf(f"{SP}/{name}_df.png", p, 0, size=TEX)
        r = np.asarray(Image.fromarray((np.clip(r, 0, 1)*255).astype(np.uint8)).resize((n, n), Image.LANCZOS)).astype(np.float32)/255.0
        print(f"== {name} self-check ref-vs-ref:", detail_report(name, ref, ref, p["lightDir"]))
        print(f"== {name} current render:", detail_report(name, r, ref, p["lightDir"]))
