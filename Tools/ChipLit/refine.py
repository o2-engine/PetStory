#!/usr/bin/env python3
"""Fits chip_lit params per chip and bakes a ROTATION-NEUTRAL residual into the albedo.

Only components that legitimately rotate with the sprite are baked:
  ball — the radially symmetric profile (invisible under rotation) + high-frequency
         shape texture; leaf — the distance-to-edge band profile + veins detail.
The directional low-frequency shading must come from the dynamic shader only.

Gate metric (matches eval_engine.py): mean per-pixel RGB error vs the reference
over the eroded chip mask, plus the share of pixels with error > 20%.
"""
import numpy as np
from PIL import Image
import os, sys, json

SP = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SP)
from pipeline import REFS, simulate, to_img, gauss, chamfer_dt

BG = np.array([26, 26, 30], np.float32) / 255.0


def erode(mask, it=2):
    m = mask.copy()
    for _ in range(it):
        m = m & np.roll(m, 1, 0) & np.roll(m, -1, 0) & np.roll(m, 1, 1) & np.roll(m, -1, 1)
    return m


def composite(img):
    a = img[..., 3:4]
    return img[..., :3] * a + BG[None, None, :] * (1 - a)


def pixel_error(render, ref):
    mask = erode(ref[..., 3] > 0.5)
    d = np.linalg.norm(composite(render) - composite(ref), axis=-1) / np.sqrt(3)
    return d[mask].mean() * 100, (d[mask] > 0.2).mean() * 100, d, mask


def load(path):
    return np.asarray(Image.open(path).convert("RGBA")).astype(np.float32) / 255.0


def render_sim(name, albedo_path, params, angle=0):
    img = simulate(albedo_path, f"{SP}/{name}_normal.png", params, angle)
    return np.asarray(img).astype(np.float32) / 255.0


BOUNDS = {
    "lightDirX": (-0.5, 0.3), "lightDirY": (0.2, 0.8),
    "ambient": (0.0, 1.2), "diffuse": (0.0, 1.5), "diffPow": (1.0, 3.5),
    "specular": (0.0, 1.2), "shininess": (8.0, 160.0),
    "specular2": (0.0, 1.2), "shininess2": (30.0, 400.0),
    "rim": (0.0, 1.0), "rimPow": (1.0, 3.5), "fill": (0.0, 1.2), "fillPow": (0.5, 8.0),
    "edgeLight": (0.0, 1.0), "edgePow": (1.0, 16.0),
    "shadowR": (0.0, 1.0), "shadowG": (0.0, 1.0), "shadowB": (0.0, 1.0),
}
KEYS = list(BOUNDS)


def vec_to_params(v):
    return {
        "lightDir": [v[0], v[1]], "ambient": v[2], "diffuse": v[3], "diffPow": v[4],
        "specular": v[5], "shininess": v[6], "specular2": v[7], "shininess2": v[8],
        "rim": v[9], "rimPow": v[10], "fill": v[11], "fillPow": v[12],
        "edgeLight": v[13], "edgePow": v[14],
        "shadowTint": [v[15], v[16], v[17]], "specColor": [1.0, 1.0, 1.0],
    }


def params_to_vec(p):
    return np.array([p["lightDir"][0], p["lightDir"][1], p["ambient"], p["diffuse"],
                     p["diffPow"], p["specular"], p["shininess"],
                     p.get("specular2", 0.3), p.get("shininess2", 120.0),
                     p["rim"], p["rimPow"], p["fill"], p.get("fillPow", 2.0),
                     p.get("edgeLight", 0.2), p.get("edgePow", 4.0),
                     *p["shadowTint"]], np.float64)


def fit_params(name, albedo_path, seed_params, iters_random=400, iters_refine=500, rng_seed=3,
               restarts=2):
    ref = load(REFS[name])
    rng = np.random.default_rng(rng_seed)
    lo = np.array([BOUNDS[k][0] for k in KEYS])
    hi = np.array([BOUNDS[k][1] for k in KEYS])

    # bright zones of the reference (painted highlights) get their own error term:
    # thin crisp arcs barely move the mean, but define the candy look
    l = 0.2126*ref[..., 0] + 0.7152*ref[..., 1] + 0.0722*ref[..., 2]
    m0 = erode(ref[..., 3] > 0.5)
    bright = m0 & (l >= np.percentile(l[m0], 92))

    def objective(v):
        render = render_sim(name, albedo_path, vec_to_params(v))
        e_mean, e_bad, dmap, _ = pixel_error(render, ref)
        e_bright = dmap[bright].mean() * 100
        return e_mean + 0.3 * e_bad + 0.5 * e_bright

    best = np.clip(params_to_vec(seed_params), lo, hi)
    best_e = objective(best)

    for restart in range(restarts):
        cur, cur_e = best.copy(), best_e
        for _ in range(iters_random // restarts):
            v = np.clip(cur + rng.normal(0, 0.25, len(KEYS)) * (hi - lo) * 0.5, lo, hi)
            e = objective(v)
            if e < cur_e:
                cur, cur_e = v, e
        step = (hi - lo) * 0.08
        for it in range(iters_refine // restarts):
            k = it % len(KEYS)
            for sign in (1, -1):
                v = cur.copy()
                v[k] = np.clip(v[k] + sign * step[k], lo[k], hi[k])
                e = objective(v)
                if e < cur_e:
                    cur, cur_e = v, e
                    break
            else:
                step[k] *= 0.7
        if cur_e < best_e:
            best, best_e = cur, cur_e
    return vec_to_params(best), best_e


def banded_profile(ratio, mask, coord, nbins, regions):
    """Band-averaged profile per channel, computed separately per region.

    coord is rotation-invariant (radius / distance-to-edge), so the result stays
    rotation-neutral; per-region profiles avoid averaging across the inner shape
    boundary, which would tint both sides toward a compromise."""
    cmax = coord[mask].max()
    bins = np.clip((coord / max(cmax, 1e-3) * nbins).astype(int), 0, nbins)
    acc = np.zeros_like(ratio)
    wsum = np.zeros(mask.shape, np.float32)
    for region_w in regions:
        rw = np.clip(region_w, 0, 1)
        sel_base = mask & (rw > 0.02)
        prof_img = np.ones_like(ratio)
        for c in range(3):
            prof = np.ones(nbins + 1, np.float32)
            for b in range(nbins + 1):
                sel = sel_base & (bins == b)
                if sel.sum() > 8:
                    w = rw[sel]
                    prof[b] = (ratio[..., c][sel] * w).sum() / max(w.sum(), 1e-4)
            sm = np.convolve(np.pad(prof, 2, mode="edge"), np.ones(5) / 5, "valid")
            prof_img[..., c] = sm[bins]
        acc += prof_img * rw[..., None]
        wsum += rw
    return np.where(wsum[..., None] > 1e-3, acc / np.maximum(wsum, 1e-3)[..., None], 1.0)


def fit_staged_ball(name, albedo_path, seed_params, rng_seed=3):
    """Two-stage fit for balls: interior terms on the inner disc first, then the
    edge terms on the rim ring, then a short joint refine."""
    ref = load(REFS[name])
    mask = erode(ref[..., 3] > 0.5)
    ys, xs = np.nonzero(mask)
    cy, cx = ys.mean(), xs.mean()
    yy, xx = np.mgrid[0:mask.shape[0], 0:mask.shape[1]]
    rr = np.hypot(yy - cy, xx - cx)
    rmax = rr[mask].max()
    inner_zone = mask & (rr < rmax * 0.62)
    ring_zone = mask & (rr >= rmax * 0.62)
    l = 0.2126*ref[..., 0] + 0.7152*ref[..., 1] + 0.0722*ref[..., 2]
    bright = mask & (l >= np.percentile(l[mask], 92))

    lo = np.array([BOUNDS[k][0] for k in KEYS])
    hi = np.array([BOUNDS[k][1] for k in KEYS])
    rng = np.random.default_rng(rng_seed)

    def zone_error(v, zone, extra_bright=0.0):
        render = render_sim(name, albedo_path, vec_to_params(v))
        _, _, dmap, _ = pixel_error(render, ref)
        e = dmap[zone].mean() * 100
        if extra_bright:
            e += extra_bright * dmap[bright].mean() * 100
        return e

    def search(v0, keys_free, zone, iters_r, iters_c, extra_bright=0.0):
        idx = [KEYS.index(k) for k in keys_free]
        best = v0.copy()
        best_e = zone_error(best, zone, extra_bright)
        for _ in range(iters_r):
            v = best.copy()
            v[idx] = np.clip(v[idx] + rng.normal(0, 0.3, len(idx)) * (hi[idx] - lo[idx]) * 0.5,
                             lo[idx], hi[idx])
            e = zone_error(v, zone, extra_bright)
            if e < best_e:
                best, best_e = v, e
        step = (hi - lo) * 0.08
        for it in range(iters_c):
            k = idx[it % len(idx)]
            for sign in (1, -1):
                v = best.copy()
                v[k] = np.clip(v[k] + sign * step[k], lo[k], hi[k])
                e = zone_error(v, zone, extra_bright)
                if e < best_e:
                    best, best_e = v, e
                    break
            else:
                step[k] *= 0.7
        return best, best_e

    v = np.clip(params_to_vec(seed_params), lo, hi)
    interior_keys = ["lightDirX", "lightDirY", "ambient", "diffuse", "diffPow",
                     "shadowR", "shadowG", "shadowB"]
    ring_keys = ["specular", "shininess", "specular2", "shininess2",
                 "rim", "rimPow", "fill", "fillPow"]
    v, _ = search(v, interior_keys, inner_zone, 200, 250)
    v, _ = search(v, ring_keys, ring_zone, 250, 300, extra_bright=0.4)
    v, e = search(v, interior_keys + ring_keys, mask, 150, 250, extra_bright=0.3)
    return vec_to_params(v), e


def symmetric_profile_ball(ratio, mask, regions):
    ys, xs = np.nonzero(mask)
    cy, cx = ys.mean(), xs.mean()
    yy, xx = np.mgrid[0:mask.shape[0], 0:mask.shape[1]]
    r = np.hypot(yy - cy, xx - cx)
    return banded_profile(ratio, mask, r, 40, regions)


def symmetric_profile_dt(ratio, mask, dtmap, regions):
    return banded_profile(ratio, mask, dtmap, 30, regions)


def bake_neutral(name, base_albedo_path, params, out_path, hf_cap=1.22, rounds=4,
                 hf_gain=None):
    """Bakes only the rotation-neutral residual: symmetric profile + capped
    high-frequency detail. The directional component is intentionally left out.
    hf_gain (0..1 map) restricts where high-frequency detail may be baked."""
    ref = load(REFS[name])
    a0 = load(base_albedo_path)
    a = a0.copy()
    mask = ref[..., 3] > 0.5
    regions = region_weights(name, mask.shape)
    if name == "leaf":
        dtmap = chamfer_dt((gauss(ref[..., 3], 2) > 0.4).astype(np.float32))
        sym_of = lambda t: symmetric_profile_dt(t, mask, dtmap, regions)
    else:
        sym_of = lambda t: symmetric_profile_ball(t, mask, regions)
    if hf_gain is None:
        hf_gain = np.ones(mask.shape, np.float32)

    tmp = f"{SP}/{name}_albedo_bake.png"
    for _ in range(rounds):
        to_img(a).save(tmp)
        r = render_sim(name, tmp, params)
        t = composite(ref) / np.maximum(composite(r), 1e-3)
        t = np.clip(t, 0.4, 2.5)
        sym = sym_of(t)
        detail = t / np.maximum(sym, 1e-3)
        detail_hf = detail / np.maximum(gauss(detail, 9), 1e-3)
        hf = 1.0 + (np.clip(detail_hf, 1.0 / 1.12, 1.12) - 1.0) * hf_gain[..., None]
        step = sym * hf
        newa = np.clip(a[..., :3] * step, 0, 1)
        # cumulative high-frequency cap keeps texture but forbids sharp baked lighting
        limit = np.clip(gauss(newa, 9), 1e-3, None) * (1.0 + (hf_cap - 1.0) * hf_gain[..., None])
        newa = np.clip(newa, 0, limit)
        a[..., :3] = np.where(mask[..., None], newa, a[..., :3])
    a[..., 3] = a0[..., 3]  # the flat base already carries the correct solid silhouette
    to_img(a).save(out_path)
    e_mean, e_bad, _, _ = pixel_error(render_sim(name, out_path, params), ref)
    return e_mean, e_bad


def ring_consistency(name, albedo_path, params):
    """Ball-only invariant: the ball BODY ring must render identically at 0 and 90 deg.
    The inner shape region (at either angle) is excluded — its texture legitimately
    rotates with the sprite."""
    r0 = render_sim(name, albedo_path, params, 0)
    r90 = render_sim(name, albedo_path, params, 90)
    ref = load(REFS[name])
    mask = erode(ref[..., 3] > 0.5, 3)
    ys, xs = np.nonzero(mask)
    cy, cx = ys.mean(), xs.mean()
    yy, xx = np.mgrid[0:mask.shape[0], 0:mask.shape[1]]
    rr = np.hypot(yy - cy, xx - cx)
    rmax = rr[mask].max()
    ring = mask & (rr > rmax * 0.55) & (rr < rmax * 0.92)
    inner = ball_hf_gain(name, mask.shape) > 0.03
    inner90 = np.asarray(Image.fromarray(inner.astype(np.uint8)*255).rotate(90)).astype(bool)
    ring &= ~(inner | inner90)
    d = np.linalg.norm(composite(r0) - composite(r90), axis=-1) / np.sqrt(3)
    return d[ring].mean() * 100


def ball_hf_gain(name, shape):
    """High-frequency detail only inside the embossed inner shape; the outer ring stays clean."""
    from pipeline import clean_mask
    inner = load(f"{SP}/{name}_inner_mask.png")[..., :3].mean(-1)
    inner = np.asarray(Image.fromarray((inner*255).astype(np.uint8)).resize(
        (shape[1], shape[0]), Image.LANCZOS)).astype(np.float32)/255.0
    inner = clean_mask(inner)
    return gauss(inner, 3)


def region_weights(name, shape):
    """Soft region split for profile baking: inner shape vs body (balls),
    veins vs blade (leaf)."""
    if name in ("red", "blue"):
        inner = ball_hf_gain(name, shape)
        return [inner, 1.0 - inner]
    veins = load(f"{SP}/leaf_veins_mask.png")[..., :3].mean(-1)
    veins = np.asarray(Image.fromarray((veins*255).astype(np.uint8)).resize(
        (shape[1], shape[0]), Image.LANCZOS)).astype(np.float32)/255.0
    veins = np.clip(gauss((veins > 0.5).astype(np.float32), 1.5)*1.5, 0, 1)
    return [veins, 1.0 - veins]


if __name__ == "__main__":
    from pipeline import PARAMS
    fitted = {}
    for name in ("red", "blue", "leaf"):
        base = f"{SP}/{name}_albedo_flat.png"
        ref_shape = load(REFS[name]).shape[:2]
        gain = ball_hf_gain(name, ref_shape) if name in ("red", "blue") else None
        params, e = fit_params(name, base, PARAMS[name])
        e_mean, e_bad = bake_neutral(name, base, params, f"{SP}/{name}_albedo_final.png", hf_gain=gain)
        params, e = fit_params(name, f"{SP}/{name}_albedo_final.png", params,
                               iters_random=120, iters_refine=250, rng_seed=5)
        e_mean, e_bad = bake_neutral(name, base, params, f"{SP}/{name}_albedo_final.png", hf_gain=gain)
        line = f"{name}: sim mean={e_mean:.2f}% bad={e_bad:.2f}%"
        if name in ("red", "blue"):
            line += f" ring0v90={ring_consistency(name, f'{SP}/{name}_albedo_final.png', params):.2f}%"
        print(line)
        fitted[name] = params
    json.dump(fitted, open(f"{SP}/fitted_params.json", "w"), indent=1, default=float)
