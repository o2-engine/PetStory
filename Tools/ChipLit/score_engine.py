#!/usr/bin/env python3
"""Scores the engine screenshot grid (chip_lit.png + manifest) against the references."""
import numpy as np
from PIL import Image
import json, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from score import ref_match, invariance, REFS

SHOTS = "/Users/andreizenkovich/work/PetStory/Bin/Mac/TestScreenshots"


def canvas_rotate_ref(ref_img, angle):
    """Reference composed onto a square canvas and rotated (sprite-style, canvas kept)."""
    w, h = ref_img.size
    side = int(np.ceil(np.hypot(w, h)))
    canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    canvas.paste(ref_img, ((side - w)//2, (side - h)//2), ref_img)
    if angle:
        canvas = canvas.rotate(angle, Image.BICUBIC, expand=False)
    return canvas


def crop_cell(shot, cell):
    side = int(np.ceil(np.hypot(cell["w"], cell["h"])))
    cx, cy = cell["cx"], cell["cy"]
    return shot.crop((cx - side//2, cy - side//2, cx - side//2 + side, cy - side//2 + side))


def main():
    shot = Image.open(f"{SHOTS}/chip_lit.png").convert("RGBA")
    manifest = json.load(open(f"{SHOTS}/chip_lit_manifest.json"))
    refs = {n: Image.open(p).convert("RGBA") for n, p in REFS.items()}

    report = {}
    for name in ("red", "blue", "leaf"):
        big = next(c for c in manifest if c["name"] == f"{name}_big")
        crop = crop_cell(shot, big)
        ref_canvas = canvas_rotate_ref(refs[name], 0).resize(crop.size, Image.LANCZOS)
        render = np.asarray(crop).astype(np.float32)/255.0
        refa = np.asarray(ref_canvas).astype(np.float32)/255.0
        render[..., 3] = refa[..., 3]  # screenshot has no alpha: borrow the reference mask
        rm = ref_match(render, refa)

        renders = {}
        for cell in manifest:
            if cell["name"] != name:
                continue
            crop = crop_cell(shot, cell)
            ref_canvas = canvas_rotate_ref(refs[name], cell["angle"]).resize(crop.size, Image.LANCZOS)
            arr = np.asarray(crop).astype(np.float32)/255.0
            arr[..., 3] = np.asarray(ref_canvas).astype(np.float32)[..., 3]/255.0
            renders[cell["angle"]] = arr
        inv = invariance(renders)
        composite = round(0.6*rm["total"] + 0.4*inv["total"], 1)
        report[name] = dict(refmatch=rm, invariance=inv, composite=composite)
        print(f"{name:5s} composite={composite:5.1f}  ref={rm}  inv={inv}")

    json.dump(report, open(f"{os.path.dirname(os.path.abspath(__file__))}/score_engine.json", "w"),
              indent=1, default=float)
    return report


if __name__ == "__main__":
    main()
