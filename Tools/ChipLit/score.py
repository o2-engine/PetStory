#!/usr/bin/env python3
"""Scoring: closeness of a rendered chip to its reference + lighting invariants under rotation.

Metrics (per chip):
  ref-match @0°  (0-100):
    - color   : mean per-pixel RGB distance inside the shared mask (weight 30)
    - profile : correlation of 24-band vertical luminance profiles (weight 25)
    - hilite  : distance between top-5% luminance centroids, in radii (weight 20)
    - ssim    : global structure similarity on luminance (weight 25)
  rotation invariants across angles (0-100):
    - lightup : luminance centroid must stay in the upper half for every angle
    - stable  : std of mean luminance across angles (shading energy must not swim)
    - struct  : un-rotating the render must match the 0° render (emboss follows sprite)
Composite = 0.6 * refmatch + 0.4 * invariance.
"""
import numpy as np
from PIL import Image
import sys, os, json

def load(path, size=None):
    img = Image.open(path).convert("RGBA")
    if size:
        img = img.resize(size, Image.LANCZOS)
    return np.asarray(img).astype(np.float32) / 255.0

def lum(a):
    return 0.2126 * a[..., 0] + 0.7152 * a[..., 1] + 0.0722 * a[..., 2]

def ssim(a, b, mask):
    mu_a, mu_b = a[mask].mean(), b[mask].mean()
    va, vb = a[mask].var(), b[mask].var()
    cov = ((a - mu_a) * (b - mu_b))[mask].mean()
    c1, c2 = 0.01 ** 2, 0.03 ** 2
    return ((2 * mu_a * mu_b + c1) * (2 * cov + c2)) / ((mu_a ** 2 + mu_b ** 2 + c1) * (va + vb + c2))

def bright_centroid(img, mask, top=0.05):
    l = lum(img).copy()
    l[~mask] = -1
    n = max(int(mask.sum() * top), 1)
    idx = np.argpartition(l.ravel(), -n)[-n:]
    ys, xs = np.unravel_index(idx, l.shape)
    return ys.mean(), xs.mean()

def mask_geom(mask):
    ys, xs = np.nonzero(mask)
    cy, cx = ys.mean(), xs.mean()
    R = max(np.hypot(ys - cy, xs - cx).max(), 1.0)
    return cy, cx, R

def ref_match(render, ref):
    mask = (ref[..., 3] > 0.5) & (render[..., 3] > 0.5)
    if mask.sum() < 100:
        return dict(color=0, profile=0, hilite=0, ssim=0, total=0)
    lr, lf = lum(render), lum(ref)

    color = np.linalg.norm(render[..., :3] - ref[..., :3], axis=-1)[mask].mean() / np.sqrt(3)
    s_color = max(0.0, 1.0 - color * 2.8)

    bands = 24
    h = mask.shape[0]
    pr, pf = [], []
    for i in range(bands):
        rows = slice(i * h // bands, (i + 1) * h // bands)
        m = mask[rows]
        if m.sum() > 10:
            pr.append(lr[rows][m].mean())
            pf.append(lf[rows][m].mean())
    pr, pf = np.array(pr), np.array(pf)
    s_prof = max(0.0, np.corrcoef(pr, pf)[0, 1]) if len(pr) > 3 else 0.0

    cy, cx, R = mask_geom(mask)
    (ry, rx), (fy, fx) = bright_centroid(render, mask), bright_centroid(ref, mask)
    s_hil = max(0.0, 1.0 - np.hypot(ry - fy, rx - fx) / R * 2.0)

    s_ssim = max(0.0, ssim(lr, lf, mask))

    total = 30 * s_color + 25 * s_prof + 20 * s_hil + 25 * s_ssim
    return dict(color=round(100 * s_color, 1), profile=round(100 * s_prof, 1),
                hilite=round(100 * s_hil, 1), ssim=round(100 * s_ssim, 1), total=round(total, 1))

def lowpass(l, sigma=9):
    img = Image.fromarray((np.clip(l, 0, 1) * 255).astype(np.uint8))
    from PIL import ImageFilter
    return np.asarray(img.filter(ImageFilter.GaussianBlur(sigma))).astype(np.float32) / 255.0

def erode(mask, it=6):
    m = mask.copy()
    for _ in range(it):
        m = m & np.roll(m, 1, 0) & np.roll(m, -1, 0) & np.roll(m, 1, 1) & np.roll(m, -1, 1)
    return m

def invariance(renders_by_angle):
    """renders_by_angle: {angle_deg: rgba array}, angle = sprite rotation.

    lightup — bright centroid stays up at every angle;
    lightfix — low-frequency shading field does NOT rotate (matches 0° field on mask overlap);
    struct  — high-frequency detail (emboss edges) DOES rotate with the sprite:
              un-rotated high-pass must match the 0° high-pass."""
    ups, means, fixes, structs = [], [], [], []
    base = renders_by_angle[0]
    bmask = base[..., 3] > 0.5
    lb = lum(base)
    lb_low, lb_high = lowpass(lb), lb - lowpass(lb, 4)
    for ang, img in sorted(renders_by_angle.items()):
        mask = img[..., 3] > 0.5
        cy, cx, R = mask_geom(mask)
        by, bx = bright_centroid(img, mask)
        d = np.hypot(by - cy, bx - cx)
        ok = (cy - by) / max(d, 1e-6) if d > R * 0.03 else 1.0
        ups.append(max(0.0, ok))
        means.append(lum(img)[mask].mean())
        if ang != 0:
            l = lum(img)
            m_fix = erode(bmask & mask)
            if m_fix.sum() > 200:
                a, b = lowpass(l)[m_fix], lb_low[m_fix]
                c = np.corrcoef(a, b)[0, 1] if a.std() > 1e-4 and b.std() > 1e-4 else 1.0
                fixes.append(max(0.0, c))
            back = np.asarray(Image.fromarray((img * 255).astype(np.uint8)).rotate(-ang, Image.BICUBIC)).astype(np.float32) / 255.0
            l2 = lum(back)
            m = erode(bmask & (back[..., 3] > 0.5))
            if m.sum() > 200:
                hb = l2 - lowpass(l2, 4)
                a, b = hb[m], lb_high[m]
                c = np.corrcoef(a, b)[0, 1] if a.std() > 1e-4 and b.std() > 1e-4 else 1.0
                structs.append(max(0.0, c))
    s_up = float(np.mean(ups))
    s_stable = max(0.0, 1.0 - float(np.std(means)) * 12.0)
    s_fix = float(np.mean(fixes)) if fixes else 1.0
    s_struct = float(np.mean(structs)) if structs else 1.0
    total = 30 * s_up + 15 * s_stable + 30 * s_fix + 25 * s_struct
    return dict(lightup=round(100 * s_up, 1), stable=round(100 * s_stable, 1),
                lightfix=round(100 * s_fix, 1), struct=round(100 * s_struct, 1), total=round(total, 1))

REFS = {
    "red":  "/Users/andreizenkovich/work/PetStory/Assets/Game field/Objects/Main/red.png",
    "blue": "/Users/andreizenkovich/work/PetStory/Assets/Game field/Objects/Main/blue.png",
    "leaf": "/Users/andreizenkovich/work/PetStory/Assets/Game field/Objects/leaf.png",
}

def score_files(files_by_name_angle, report_path=None):
    """files_by_name_angle: {name: {angle: path}}; renders may be any size, resized to ref."""
    report = {}
    for name, by_angle in files_by_name_angle.items():
        ref = load(REFS[name])
        size = (ref.shape[1], ref.shape[0])
        renders = {ang: load(p, size) for ang, p in by_angle.items()}
        rm = ref_match(renders[0], ref)
        inv = invariance(renders) if len(renders) > 1 else dict(total=0)
        composite = round(0.6 * rm["total"] + 0.4 * inv["total"], 1) if len(renders) > 1 else rm["total"]
        report[name] = dict(refmatch=rm, invariance=inv, composite=composite)
    if report_path:
        json.dump(report, open(report_path, "w"), indent=1, default=float)
    for name, r in report.items():
        print(f"{name:5s} composite={r['composite']:5.1f}  ref={r['refmatch']}  inv={r['invariance']}")
    return report

if __name__ == "__main__":
    sp = os.path.dirname(os.path.abspath(__file__))
    mode = sys.argv[1] if len(sys.argv) > 1 else "sim"
    if mode == "sim":
        sys.path.insert(0, sp)
        from pipeline import simulate, DEFAULT_PARAMS
        files = {}
        for name in ("red", "blue", "leaf"):
            files[name] = {}
            for ang in (0, 45, 90, 135, 180, 225, 270, 315):
                out = f"{sp}/{name}_sim{ang}.png"
                simulate(f"{sp}/{name}_albedo.png", f"{sp}/{name}_normal.png", DEFAULT_PARAMS, ang, out)
                files[name][ang] = out
        score_files(files, f"{sp}/score_sim.json")
