#!/usr/bin/env python3
"""Gate metric on the engine render + Out/ artifacts for the contributor.

Usage: eval_engine.py <iteration-label> [note...]
Reads Bin/Mac/TestScreenshots/chip_lit.png + manifest, measures the big 0-degree
cells against the references, writes Out/: per-chip ref|render|diff sheets,
metrics.json, STATUS.md log. Exit code 0 when every chip passes the gate.
"""
import numpy as np
from PIL import Image, ImageDraw
import json, os, sys, datetime

SP = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SP)
from pipeline import REFS
from refine import pixel_error, BG

REPO = "/Users/andreizenkovich/work/PetStory"
OUT = f"{REPO}/Out"
SHOTS = f"{REPO}/Bin/Mac/TestScreenshots"

GATE_MEAN = 3.5   # % mean per-pixel RGB error (red/blue; leaf reported non-gating)
GATE_BAD = 0.5    # % of pixels with error > 20%
GATE_RING = 2.5   # % mean error of the ball ring between 0 and 90 degrees (no baked directional light)
GATING = ("red", "blue")  # per user request the leaf is parked for now
GATE_SHADOW = 85.0
GATE_CRESCENT = 75.0
GATE_DOTS = 80.0
GATE_SDEPTH = 70.0   # icon shadow depth must match the reference
GATE_SHARP = 70.0    # light features must not be sharper/softer than the displayed reference


def crop_cell(shot, cell):
    side = int(np.ceil(np.hypot(cell["w"], cell["h"])))
    cx, cy = cell["cx"], cell["cy"]
    return shot.crop((cx - side//2, cy - side//2, cx - side//2 + side, cy - side//2 + side))


def crop_big_cell(shot, manifest, name):
    return crop_cell(shot, next(c for c in manifest if c["name"] == f"{name}_big"))


def ring_consistency_engine(shot, manifest, name):
    """Mean error of the ball BODY ring between the 0 and 90 degree grid cells.
    The inner shape (at either angle) is excluded: its texture rotates legitimately."""
    c0 = next(c for c in manifest if c["name"] == name and c["angle"] == 0)
    c90 = next(c for c in manifest if c["name"] == name and c["angle"] == 90)
    a = np.asarray(crop_cell(shot, c0).convert("RGB")).astype(np.float32)/255.0
    b = np.asarray(crop_cell(shot, c90).convert("RGB")).astype(np.float32)/255.0
    side = a.shape[0]
    radius = c0["w"]*0.5
    yy, xx = np.mgrid[0:side, 0:side]
    rr = np.hypot(yy - side/2, xx - side/2)
    ring = (rr > radius*0.55) & (rr < radius*0.92)
    from refine import ball_hf_gain
    inner_ref = ball_hf_gain(name, (210, 210)) > 0.03
    im = Image.fromarray(inner_ref.astype(np.uint8)*255).resize((int(radius*2),)*2, Image.NEAREST)
    canvas = Image.new("L", (side, side), 0)
    canvas.paste(im, ((side - im.size[0])//2, (side - im.size[1])//2))
    inner = np.asarray(canvas) > 127
    inner90 = np.asarray(canvas.rotate(90)) > 127
    ring &= ~(inner | inner90)
    d = np.linalg.norm(a - b, axis=-1)/np.sqrt(3)
    return float(d[ring].mean()*100)


def register(render_rgb, ref_rgba, search=3):
    """Finds the integer shift of the render that best aligns it with the reference."""
    mask = ref_rgba[..., 3] > 0.5
    ref_c = ref_rgba[..., :3] * ref_rgba[..., 3:4] + BG[None, None, :] * (1 - ref_rgba[..., 3:4])
    best, best_shift = None, (0, 0)
    for dy in range(-search, search + 1):
        for dx in range(-search, search + 1):
            shifted = np.roll(render_rgb, (dy, dx), (0, 1))
            e = np.abs(shifted - ref_c)[mask].mean()
            if best is None or e < best:
                best, best_shift = e, (dy, dx)
    return np.roll(render_rgb, best_shift, (0, 1)), best_shift


def main():
    label = sys.argv[1] if len(sys.argv) > 1 else "iter"
    note = " ".join(sys.argv[2:])
    os.makedirs(OUT, exist_ok=True)

    shot = Image.open(f"{SHOTS}/chip_lit.png").convert("RGBA")
    manifest = json.load(open(f"{SHOTS}/chip_lit_manifest.json"))

    results = {}
    all_pass = True
    for name in ("red", "blue", "leaf"):
        ref_img = Image.open(REFS[name]).convert("RGBA")
        w, h = ref_img.size
        crop = crop_big_cell(shot, manifest, name).resize(
            (int(np.ceil(np.hypot(w, h))),)*2, Image.LANCZOS)
        side = crop.size[0]
        canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
        canvas.paste(ref_img, ((side - w)//2, (side - h)//2), ref_img)
        ref = np.asarray(canvas).astype(np.float32)/255.0
        render = np.asarray(crop).astype(np.float32)/255.0

        render_rgb, shift = register(render[..., :3], ref)
        render_reg = np.concatenate([render_rgb, ref[..., 3:4]], -1)  # alpha from ref: bg is shared
        e_mean, e_bad, dmap, mask = pixel_error(render_reg, ref)
        ring = ring_consistency_engine(shot, manifest, name) if name in ("red", "blue") else None
        details = None
        if name in ("red", "blue"):
            from details import detail_report
            import json as _json
            sp = os.path.dirname(os.path.abspath(__file__))
            light = _json.load(open(f"{sp}/{name}_sdf_params.json"))["lightDir"]
            # detail masks are built at the chip's native size: crop the centered chip
            off = (side - w)//2
            details = detail_report(name, render_reg[off:off + h, off:off + w],
                                    ref[off:off + h, off:off + w], light)
        ok = e_mean <= GATE_MEAN and e_bad <= GATE_BAD and (ring is None or ring <= GATE_RING)
        if details is not None:
            ok &= details["shadow"] >= GATE_SHADOW and details["crescent"] >= GATE_CRESCENT \
                and details["dots"] >= GATE_DOTS and details["shadow_depth"] >= GATE_SDEPTH \
                and details["sharp_rim"] >= GATE_SHARP and details["sharp_icon"] >= GATE_SHARP
        if name in GATING:
            all_pass &= ok
        results[name] = dict(mean=round(float(e_mean), 2), bad=round(float(e_bad), 2),
                             gate_mean=GATE_MEAN, gate_bad=GATE_BAD, shift=list(shift),
                             ring=None if ring is None else round(ring, 2), gate_ring=GATE_RING,
                             details=details, passed=bool(ok))

        # Out sheet: reference | engine render | error heatmap
        heat = np.zeros((side, side, 3), np.float32) + 0.12
        hm = np.clip(dmap/0.35, 0, 1)
        heat[mask] = np.stack([hm[mask], 0.1 + 0.2*(1 - hm[mask]), 1 - hm[mask]], -1)
        ref_v = Image.fromarray((np.clip(ref[..., :3]*ref[..., 3:4] + BG*(1 - ref[..., 3:4]), 0, 1)*255).astype(np.uint8))
        ren_v = Image.fromarray((np.clip(render_rgb, 0, 1)*255).astype(np.uint8))
        heat_v = Image.fromarray((heat*255).astype(np.uint8))
        sheet = Image.new("RGB", (side*3 + 20, side + 34), (24, 24, 28))
        for i, im in enumerate((ref_v, ren_v, heat_v)):
            sheet.paste(im, (i*(side + 10), 30))
        d = ImageDraw.Draw(sheet)
        d.text((6, 8), f"{name}  REFERENCE", fill=(200, 200, 200))
        d.text((side + 16, 8), f"ENGINE  mean={e_mean:.2f}% (gate {GATE_MEAN}%)  bad={e_bad:.2f}% (gate {GATE_BAD}%)  {'PASS' if ok else 'FAIL'}",
               fill=(120, 230, 120) if ok else (250, 120, 120))
        d.text((2*side + 26, 8), "ERROR HEATMAP", fill=(200, 200, 200))
        sheet.save(f"{OUT}/{name}_compare.png")

    # rotation strip: every chip at every grid angle, for the visual rotation check
    rows = []
    for name in ("red", "blue", "leaf"):
        cells = sorted((c for c in manifest if c["name"] == name), key=lambda c: c["angle"])
        strip = Image.new("RGB", (8*190, 190), (26, 26, 30))
        for i, c in enumerate(cells):
            crop = crop_cell(shot, c).convert("RGB")
            crop.thumbnail((185, 185), Image.LANCZOS)
            strip.paste(crop, (i*190 + (190 - crop.width)//2, (190 - crop.height)//2))
        rows.append(strip)
    strip_sheet = Image.new("RGB", (8*190, 3*190), (26, 26, 30))
    for i, r in enumerate(rows):
        strip_sheet.paste(r, (0, i*190))
    strip_sheet.save(f"{OUT}/rotation_strip.png")

    json.dump(results, open(f"{OUT}/metrics.json", "w"), indent=1, default=float)

    line = f"| {label} | " + " | ".join(
        f"{results[n]['mean']:.2f}% / {results[n]['bad']:.2f}% {'✅' if results[n]['passed'] else '❌'}"
        for n in ("red", "blue", "leaf")) + f" | {note} |"
    status_path = f"{OUT}/STATUS.md"
    if not os.path.exists(status_path):
        with open(status_path, "w") as f:
            f.write("# ChipLit — сближение с референсом\n\n"
                    f"Порог: средняя попиксельная ошибка ≤ {GATE_MEAN}%, пикселей с ошибкой >20% — ≤ {GATE_BAD}%.\n"
                    "Формат ячеек: mean / bad.\n\n"
                    "| итерация | red | blue | leaf | заметка |\n|---|---|---|---|---|\n")
    with open(status_path, "a") as f:
        f.write(line + "\n")

    for n in ("red", "blue", "leaf"):
        r = results[n]
        ring = f"ring0v90={r['ring']:5.2f}% (≤{GATE_RING}%)  " if r["ring"] is not None else ""
        det = ""
        if r.get("details"):
            d = r["details"]
            det = (f"shadow={d['shadow']:5.1f}  depth={d['shadow_depth']:5.1f} "
                   f"({d['depth_ref']}→{d['depth_render']})  crescent={d['crescent']:5.1f}  "
                   f"dots={d['dots']:5.1f}  sharpRim={d['sharp_rim']:5.1f}({d['sharp_rim_ratio']})  "
                   f"sharpIcon={d['sharp_icon']:5.1f}({d['sharp_icon_ratio']})  ")
        print(f"{n:5s} mean={r['mean']:6.2f}% (≤{GATE_MEAN}%)  bad={r['bad']:5.2f}% (≤{GATE_BAD}%)  "
              f"{ring}{det}shift={r['shift']}  {'PASS' if r['passed'] else 'FAIL'}")
    print("GATE:", "PASS" if all_pass else "FAIL")
    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
