#!/usr/bin/env python3
"""SDF chip model: distance-field textures + the layered shader simulator.

DF texture (per chip): R = base circle signed DF, G = icon signed DF,
B = 0, A = silhouette alpha. Distances encoded as 0.5 + d_px/(2*DF_RANGE),
positive inside. Texture size TEX px, sprite renders at REF size.
"""
import numpy as np
from PIL import Image
import os, sys, json

SP = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SP)
from pipeline import REFS, gauss, chamfer_dt, clean_mask, load_rgba, to_img

TEX = 420
DF_RANGE = 128.0  # px, encoded half-range


def signed_df(mask):
    """Signed chamfer distance in pixels: positive inside, negative outside."""
    inside = chamfer_dt(mask)
    outside = chamfer_dt(1.0 - mask)
    return inside - outside



def load_df(path, size=None):
    """DF texture loader: channels resized separately — PIL premultiplies RGBA on
    resize, which crushes the R/G distance channels where the baked-gradient alpha
    channel is near zero."""
    raw = np.asarray(Image.open(path))
    if size and size != raw.shape[0]:
        chans = [np.asarray(Image.fromarray(raw[..., c]).resize((size, size), Image.LANCZOS))
                 for c in range(raw.shape[2])]
        raw = np.stack(chans, -1)
    return raw.astype(np.float32)/255.0


def build_df(name):
    ref_a = load_rgba(REFS[name], (TEX, TEX))[..., 3]
    base_mask = (ref_a > 0.5).astype(np.float32)
    ys, xs = np.nonzero(base_mask)
    cy, cx = ys.mean(), xs.mean()
    R = np.hypot(ys - cy, xs - cx).max()
    yy, xx = np.mgrid[0:TEX, 0:TEX]
    base_sd = R - np.hypot(yy - cy, xx - cx)  # analytic circle: positive inside

    icon = load_rgba(f"{SP}/{name}_inner_mask.png", (TEX, TEX))[..., :3].mean(-1)
    icon = clean_mask(icon)
    sd_sigma = 1.4 if name == "orange" else 2.0
    icon_sd = gauss(signed_df(np.where(icon > 0.5, 1.0, 0.0).astype(np.float32)), sd_sigma)

    # smoothed outward gradient of the icon DF, baked so the shader needs no runtime
    # derivatives (the raw gradient angle combs the angular LUT on the wavy contour);
    # stored in sprite space (y up), independent of sampling orientation
    grad_sigma = 2.5
    gyd, gxd = np.gradient(gauss(icon_sd, grad_sigma))
    gdir = np.stack([gxd, -gyd], -1)
    gdir = -gdir/np.maximum(np.linalg.norm(gdir, axis=-1, keepdims=True), 1e-6)

    out = np.zeros((TEX, TEX, 4), np.float32)
    out[..., 0] = np.clip(0.5 + base_sd/(2*DF_RANGE), 0, 1)
    out[..., 1] = np.clip(0.5 + icon_sd/(2*DF_RANGE), 0, 1)
    out[..., 2] = gdir[..., 0]*0.5 + 0.5
    out[..., 3] = gdir[..., 1]*0.5 + 0.5
    to_img(out).save(f"{SP}/{name}_df.png")

    # alpha is derived from the base DF in the shader: fit offset/softness to the
    # hand-drawn reference silhouette (its edge is not a perfect analytic circle)
    band = np.abs(base_sd) < 6
    best = (0.0, 1.5, 1e9)
    for off in np.linspace(-2.5, 2.5, 26):
        for soft in np.linspace(0.6, 3.5, 15):
            a = np.clip((base_sd - (off - soft))/(2*soft), 0, 1)
            a = a*a*(3 - 2*a)
            e = np.abs(a - ref_a)[band].mean()
            if e < best[2]:
                best = (float(off), float(soft), float(e))
    meta = dict(cx=float(cx)/TEX, cy=float(cy)/TEX, R=float(R)/TEX,
                alphaOff=best[0], alphaSoft=best[1])
    json.dump(meta, open(f"{SP}/{name}_df.json", "w"))
    print(f"{name}: df saved, center=({meta['cx']:.3f},{meta['cy']:.3f}) R={meta['R']:.3f}")
    return meta


# ---------------- layered simulator (reference implementation of chip_sdf.frag) --------

def smoothstep(e0, e1, x):
    t = np.clip((x - e0)/np.maximum(e1 - e0, 1e-6), 0, 1)
    return t*t*(3 - 2*t)


def band(d, center, width, soft):
    """Soft band around d = center (px), width/soft in px."""
    return smoothstep(center - width/2 - soft, center - width/2, d) * \
           (1 - smoothstep(center + width/2, center + width/2 + soft, d))


def render_sdf(df_path, params, angle_deg=0.0, size=None):
    """Renders the chip in sprite space; world light rotated into sprite space by -angle."""
    dfm = load_df(df_path, size)
    n = dfm.shape[0]
    px = DF_RANGE*2  # encoded px per unit value
    base_d = (dfm[..., 0] - 0.5)*px * (n/TEX)  # scale distances if resized
    icon_d = (dfm[..., 1] - 0.5)*px * (n/TEX)
    meta0 = json.load(open(df_path.replace(".png", ".json")))
    a_off = meta0.get("alphaOff", 0.0)*(n/TEX)
    a_soft = max(meta0.get("alphaSoft", 1.5)*(n/TEX), 0.4)
    alpha = smoothstep(a_off - a_soft, a_off + a_soft, base_d)
    gdir_baked = dfm[..., 2:4]*2.0 - 1.0
    gdir_baked = gdir_baked/np.maximum(np.linalg.norm(gdir_baked, axis=-1, keepdims=True), 1e-6)

    p = params
    th = np.deg2rad(angle_deg)
    # world -> sprite: rotate by -angle
    def w2s(v):
        c, s = np.cos(-th), np.sin(-th)
        return np.array([v[0]*c - v[1]*s, v[0]*s + v[1]*c], np.float32)

    L = np.array(p["lightDir"], np.float32)
    L = L/max(np.linalg.norm(L), 1e-6)
    Ls = w2s(L)
    Bo = np.array(p["bounceDir"], np.float32)
    Bo = Bo/max(np.linalg.norm(Bo), 1e-6)
    Bs = w2s(Bo)

    meta = json.load(open(df_path.replace(".png", ".json")))
    cy, cx = meta["cy"]*n, meta["cx"]*n
    Rpx = meta["R"]*n
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    rel = np.stack([(xx - cx), -(yy - cy)], -1)  # sprite space, y up
    rad = rel/np.maximum(np.linalg.norm(rel, axis=-1, keepdims=True), 1e-6)

    az_l = rad @ Ls          # +1 on lit edge, -1 on shadow edge
    az_b = rad @ Bs          # bounce azimuth

    # ---- base body: linear gradient along the light in sprite space
    t = np.clip(0.5 + (rel @ Ls)/(Rpx*p["gradSpread"]), 0, 1) ** p["gradPow"]
    top = np.array(p["baseTop"]); bot = np.array(p["baseBottom"])
    color = bot[None, None, :]*(1 - t[..., None]) + top[None, None, :]*t[..., None]

    # ---- dark crescent on the shadow side edge
    m = band(base_d, p["darkPos"], p["darkWidth"], p["darkSoft"]) * \
        smoothstep(p["azEdge0"], p["azEdge1"], -az_l) * p["darkStrength"]
    color = color*(1 - m[..., None]) + np.array(p["darkColor"])[None, None, :]*m[..., None]

    # ---- light crescent on the lit side edge
    m = band(base_d, p["litPos"], p["litWidth"], p["litSoft"]) * \
        smoothstep(p["azEdge0"], p["azEdge1"], az_l) * p["litStrength"]
    color = color*(1 - m[..., None]) + np.array(p["litColor"])[None, None, :]*m[..., None]

    # ---- complex bottom-right bounce: wide soft glow + narrow bright line
    m = band(base_d, p["bounceWidePos"], p["bounceWideWidth"], p["bounceWideSoft"]) * \
        smoothstep(p["azB0"], p["azB1"], az_b) * p["bounceWideStrength"]
    color = color*(1 - m[..., None]) + np.array(p["bounceColor"])[None, None, :]*m[..., None]
    m = band(base_d, p["bounceLinePos"], p["bounceLineWidth"], p["bounceLineSoft"]) * \
        smoothstep(p["azB0"], p["azB1"], az_b) * p["bounceLineStrength"]
    color = color*(1 - m[..., None]) + np.array(p["bounceLineColor"])[None, None, :]*m[..., None]

    # ---- reference rim LUT: baked (light-relative azimuth x edge distance) coloring
    lut_path = df_path.replace("_df.png", "_lut.png")
    rim_blend = p.get("rimLutBlend", 0.0)
    icon_lut_blend = p.get("iconLutBlend", 0.0)
    if (rim_blend > 0 or icon_lut_blend > 0) and os.path.exists(lut_path):
        lut_img = np.asarray(Image.open(lut_path)).astype(np.float32)/255.0
        lut = lut_img[..., :3]
        if lut.shape[1] != LUT_W:  # saved padded for the GPU: strip the wrap columns
            lut = lut[:, 1:1 + LUT_W]
        ang_b = np.arctan2(rad[..., 0]*Ls[1] - rad[..., 1]*Ls[0], rad @ Ls)
        if rim_blend > 0:
            dn = base_d/(p.get("rimRange", LUT_RIM_RANGE)*(n/TEX))
            lutc = sample_lut(lut, 1, 9, ang_b, dn)
            fade = (1.0 - smoothstep(0.75, 1.0, dn))*smoothstep(-3.0, 1.0, base_d)*rim_blend
            color = color*(1 - fade[..., None]) + lutc*fade[..., None]

    # ---- icon drop shadow: simply the icon silhouette offset down-right (world dir),
    # dark with a soft edge, covered by the icon itself
    sdir = np.array(p.get("shadowDir", [0.6, -0.8]), np.float32)
    sdir = sdir/max(np.linalg.norm(sdir), 1e-6)
    off = w2s(sdir)*p["shadowOffset"]
    oy, ox = off[1], -off[0]
    sy = np.clip(yy + oy, 0, n - 1).astype(int)
    sx = np.clip(xx + ox, 0, n - 1).astype(int)
    icon_off_d = icon_d[sy, sx]
    sh = smoothstep(-p["shadowSoft"], p["shadowSoft"], icon_off_d) * p["shadowStrength"]
    icon_mask = smoothstep(-p["iconAA"], p["iconAA"], icon_d)
    sh = sh*(1 - icon_mask)
    color = color*(1 - sh[..., None]) + np.array(p["shadowColor"])[None, None, :]*sh[..., None]

    # ---- glint on the base: its own world direction (measured from the reference)
    Gd = np.array(p.get("glintDir", p["lightDir"]), np.float32)
    Gd = Gd/max(np.linalg.norm(Gd), 1e-6)
    Gs = w2s(Gd)
    gpos = np.array([cx, cy]) + np.array([Gs[0], -Gs[1]])*Rpx*p["glintRad"]
    gd = np.hypot((xx - gpos[0])/p["glintRx"], (yy - gpos[1])/p["glintRy"])
    g = np.clip(1 - gd, 0, 1) ** p["glintPow"] * p["glintStrength"]
    color = color + (np.array(p["glintColor"])[None, None, :] - color)*g[..., None]

    # ---- icon: gradient fill
    t2 = np.clip(0.5 + (rel @ Ls)/(Rpx*p["iconGradSpread"]), 0, 1) ** p["iconGradPow"]
    itop = np.array(p["iconTop"]); ibot = np.array(p["iconBottom"])
    icol = ibot[None, None, :]*(1 - t2[..., None]) + itop[None, None, :]*t2[..., None]

    # icon edge azimuth from the baked smoothed DF gradient (raw runtime gradients
    # comb the angular lookups on the wavy contour)
    gdir = gdir_baked
    iaz = gdir @ Ls

    # lit inner edge (top-left), shadow inner edge (bottom-right), bounce line opposite
    m = band(icon_d, p["iconLitPos"], p["iconLitWidth"], p["iconLitSoft"]) * \
        smoothstep(p["iazEdge0"], p["iazEdge1"], iaz) * p["iconLitStrength"]
    icol = icol*(1 - m[..., None]) + np.array(p["iconLitColor"])[None, None, :]*m[..., None]
    m = band(icon_d, p["iconShadePos"], p["iconShadeWidth"], p["iconShadeSoft"]) * \
        smoothstep(p["iazEdge0"], p["iazEdge1"], -iaz) * p["iconShadeStrength"]
    icol = icol*(1 - m[..., None]) + np.array(p["iconShadeColor"])[None, None, :]*m[..., None]
    m = band(icon_d, p["iconBouncePos"], p["iconBounceWidth"], p["iconBounceSoft"]) * \
        smoothstep(p["iazEdge0"], p["iazEdge1"], gdir @ Bs) * p["iconBounceStrength"]
    icol = icol*(1 - m[..., None]) + np.array(p["iconBounceColor"])[None, None, :]*m[..., None]

    # icon glint: crisp DF arc on the lit side
    m = band(icon_d, p["iconGlintPos"], p["iconGlintWidth"], p["iconGlintSoft"]) * \
        smoothstep(p["iglAz0"], p["iglAz1"], iaz) * p["iconGlintStrength"]
    icol = icol*(1 - m[..., None]) + np.array(p["iconGlintColor"])[None, None, :]*m[..., None]

    # ---- icon edge LUT: baked reference coloring by (grad-to-light angle x depth)
    if icon_lut_blend > 0 and os.path.exists(lut_path):
        gd2 = gdir_baked
        ang_i = np.arctan2(gd2[..., 0]*Ls[1] - gd2[..., 1]*Ls[0], gd2 @ Ls)
        dni = icon_d/(LUT_ICON_RANGE*(n/TEX))
        lutci = sample_lut(lut, 14, 9, ang_i, dni)
        fadei = (1.0 - smoothstep(0.7, 1.0, dni))*smoothstep(-1.0, 1.5, icon_d)*icon_lut_blend
        icol = icol*(1 - fadei[..., None]) + lutci*fadei[..., None]

    color = color*(1 - icon_mask[..., None]) + icol*icon_mask[..., None]

    # ---- round glint dots: measured bright peaks of the reference, world-anchored
    # (each dot = brightest pixel enlarged and feathered, as hand-drawn art does)
    dot_color = np.array(p.get("dotColor", [1.0, 1.0, 1.0]))
    for dot in p.get("dots", []):
        ddir = np.array(dot[0:2], np.float32)
        ddir = ddir/max(np.linalg.norm(ddir), 1e-6)
        Ds = w2s(ddir)
        dist, radius, strength = dot[2]*(n/TEX), dot[3]*(n/TEX), dot[4]
        dpos = np.array([cx, cy]) + np.array([Ds[0], -Ds[1]])*dist
        dd = np.hypot(xx - dpos[0], yy - dpos[1])/max(radius, 1e-3)
        g = (1.0 - smoothstep(0.0, 1.0, dd)) ** 1.5 * strength
        color = color*(1 - g[..., None]) + dot_color[None, None, :]*g[..., None]

    out = np.concatenate([np.clip(color, 0, 1), alpha[..., None]], -1)
    return out


LUT_W = 64          # azimuth bins (relative to the light direction)
LUT_RIM_RANGE = 40.0    # px of base_d covered by the rim LUT
LUT_ICON_RANGE = 20.0   # px of icon_d covered by the icon LUT
LUT_ROWS = 24       # rows 1..9 = base rim, rows 14..22 = icon edge


def build_rim_lut(name, light, rim_range=LUT_RIM_RANGE):
    """Bakes the reference's rim coloring into a (light-relative azimuth x distance) LUT.
    Median per bin suppresses the small glint dots (they stay explicit)."""
    ref = load_rgba(REFS[name])
    n = ref.shape[0]
    df = load_df(f"{SP}/{name}_df.png", n)
    scale = n/TEX
    base_d = (df[..., 0] - 0.5)*DF_RANGE*2*scale
    icon_d = (df[..., 1] - 0.5)*DF_RANGE*2*scale
    meta = json.load(open(f"{SP}/{name}_df.json"))
    cy, cx = meta["cy"]*n, meta["cx"]*n
    yy, xx = np.mgrid[0:n, 0:n].astype(np.float32)
    rel = np.stack([xx - cx, -(yy - cy)], -1)
    rad = rel/np.maximum(np.linalg.norm(rel, axis=-1, keepdims=True), 1e-6)
    L = np.array(light, np.float32)
    L = L/max(np.linalg.norm(L), 1e-6)
    ang_base = np.arctan2(rad[..., 0]*L[1] - rad[..., 1]*L[0], rad @ L)  # signed angle to light

    # same gradient convention as the baked DF channels: gdir = normalize((-gxd, gyd))
    gyd, gxd = np.gradient(gauss(icon_d, 2.5))
    gdir = np.stack([-gxd, gyd], -1)
    gdir = gdir/np.maximum(np.linalg.norm(gdir, axis=-1, keepdims=True), 1e-6)
    ang_icon = np.arctan2(gdir[..., 0]*L[1] - gdir[..., 1]*L[0], gdir @ L)

    lut = np.zeros((LUT_ROWS, LUT_W, 3), np.float32)
    scale_px = 1.0/scale  # ref px -> TEX px

    def fill(rows, dmap, ang, dmin, dmax, sel_extra):
        nrows = len(rows)
        for ri, row in enumerate(rows):
            d0 = dmin + (dmax - dmin)*ri/nrows
            d1 = dmin + (dmax - dmin)*(ri + 1)/nrows
            for ai in range(LUT_W):
                a0 = -np.pi + ai*(2*np.pi/LUT_W)
                sel = (dmap*scale_px >= d0) & (dmap*scale_px < d1) & \
                      (ang >= a0) & (ang < a0 + 2*np.pi/LUT_W) & sel_extra
                if sel.sum() > 2:
                    lut[row, ai] = np.median(ref[..., :3][sel], 0)
                elif ai > 0:
                    lut[row, ai] = lut[row, ai - 1]
        # fill empty leading bins from the wrap side, then smooth along azimuth
        for row in rows:
            col = lut[row]
            zero = (col.sum(-1) == 0)
            if zero.any() and not zero.all():
                for ai in np.where(zero)[0]:
                    lut[row, ai] = lut[row, (ai - 1) % LUT_W]
            k = np.array([0.12, 0.76, 0.12])  # light touch: heavy smoothing softens the crescents
            for c in range(3):
                lut[row, :, c] = np.convolve(np.pad(col[:, c], 1, mode="wrap"), k, "valid")

    alpha_ok = ref[..., 3] > 0.6
    fill(range(1, 10), base_d, ang_base, 0.0, rim_range, alpha_ok & (icon_d*scale_px < -3))
    fill(range(14, 23), icon_d, ang_icon, 0.0, LUT_ICON_RANGE, alpha_ok & (icon_d > 0))
    lut[0] = lut[1]; lut[10] = lut[9]           # guard rows against bilinear bleed
    lut[13] = lut[14]; lut[23] = lut[22]
    # horizontal wrap padding for GPU clamp samplers: [last, 0..63, first]
    padded = np.concatenate([lut[:, -1:], lut, lut[:, :1]], 1)
    to_img(np.concatenate([padded, np.ones((*padded.shape[:2], 1), np.float32)], -1)).save(f"{SP}/{name}_lut.png")
    return lut


def sample_lut(lut, row0, nrows, ang, dnorm):
    """Bilinear LUT sample; ang in [-pi, pi], dnorm in [0, 1]."""
    h, w = lut.shape[:2]
    u = (ang + np.pi)/(2*np.pi)*LUT_W - 0.5
    v = row0 + np.clip(dnorm, 0, 1)*(nrows - 1)
    u0 = np.floor(u).astype(int); fu = u - u0
    v0 = np.floor(v).astype(int); fv = np.clip(v - v0, 0, 1)
    u0m = u0 % LUT_W; u1m = (u0 + 1) % LUT_W
    v0c = np.clip(v0, 0, h - 1); v1c = np.clip(v0 + 1, 0, h - 1)
    c = (lut[v0c, u0m]*(1 - fu)[..., None] + lut[v0c, u1m]*fu[..., None])*(1 - fv)[..., None] + \
        (lut[v1c, u0m]*(1 - fu)[..., None] + lut[v1c, u1m]*fu[..., None])*fv[..., None]
    return c


if __name__ == "__main__":
    for n in ("red", "blue"):
        build_df(n)
