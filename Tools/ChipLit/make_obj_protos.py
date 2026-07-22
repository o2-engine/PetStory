#!/usr/bin/env python3
"""Generates Prefabs/Obj{Name}.proto for the free-form field objects: a dynamic
RigidBody with a composite collider — circles greedily covering the silhouette
(from its distance field), each as a child actor with a CircleCollider. The image
and the ObjectChipComponent marker sit on the root; no cursor listener, so the
object can't be clicked like regular chips."""
import json, os, sys
import numpy as np

SP = os.path.dirname(os.path.abspath(__file__))
REPO = "/Users/andreizenkovich/work/PetStory"
OBJ = f"{REPO}/Assets/Game field/Objects"
PREFABS = f"{REPO}/Assets/Prefabs"
SCALE = 0.8
MAX_CIRCLES = 5
BASE_ID = 77200000000000001

sys.path.insert(0, SP)
from objects_normal import NAMES, load_rgba, signed_df
from pipeline import gauss


def meta_id(path):
    return json.load(open(path))["Value"]["mId"]


def cover_circles(name):
    """Greedy circle cover of the silhouette: biggest inscribed circle first,
    then the biggest inscribed circle of the still-uncovered part."""
    ref = load_rgba(f"{OBJ}/{name}.png")
    h, w = ref.shape[:2]
    mask = (ref[..., 3] > 0.5).astype(np.float32)
    sd = gauss(signed_df(mask), 1.5)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)

    circles = []
    uncovered = mask > 0.5
    for _ in range(MAX_CIRCLES):
        vals = np.where(uncovered, sd, 0.0)
        r = float(vals.max())
        if r < 14 or (circles and r < 0.38*circles[0][2]):
            break
        cy, cx = np.unravel_index(int(vals.argmax()), vals.shape)
        circles.append((float(cx), float(cy), r))
        uncovered &= np.hypot(xx - cx, yy - cy) > r*0.85
    # actor-relative offsets, sprite space (y up), scaled to the proto size
    return [((cx - w/2)*SCALE, -(cy - h/2)*SCALE, r*SCALE) for cx, cy, r in circles], (w, h)


def ids_for(index):
    base = BASE_ID + index*20
    return base  # actor id; components/children get base+1..


def make(name, index):
    circles, (w, h) = cover_circles(name)
    base = ids_for(index)
    uid = [base]

    def next_id():
        uid[0] += 1
        return uid[0]

    children = []
    for ci, (ox, oy, r) in enumerate(circles):
        children.append({"Type": "o2::Actor", "Data": {
            "Id": next_id(),
            "mName": f"col{ci}",
            "Transform": {"position": {"x": round(ox, 1), "y": round(oy, 1)},
                          "size": {"x": round(r*2, 1), "y": round(r*2, 1)},
                          "scale": {"x": 1.0, "y": 1.0},
                          "pivot": {"x": 0.5, "y": 0.5}},
            "Components": [
                {"Type": "o2::CircleCollider", "Data": {
                    "mRadius": round(r, 1),
                    "mFitByActor": False,
                    "mId": next_id()}},
            ]}})

    proto = {"mActor": {"Type": "o2::RigidBody", "Value": {
        "Id": base,
        "mName": f"Obj{name.capitalize()}",
        "Transform": {"size": {"x": round(w*SCALE, 1), "y": round(h*SCALE, 1)},
                      "scale": {"x": 1.0, "y": 1.0},
                      "pivot": {"x": 0.5, "y": 0.5}},
        "Components": [
            {"Type": "o2::ImageComponent", "Data": {
                "mColor": {"r": 255, "g": 255, "b": 255, "a": 255},
                "mImageAsset": {"id": meta_id(f"{OBJ}/{name}_alb.png.meta"),
                                "path": f"Game field/Objects/{name}_alb.png"},
                "mMaterialAsset": {"id": meta_id(f"{OBJ}/{name}_obj.mat.meta"),
                                   "path": f"Game field/Objects/{name}_obj.mat"},
                "mId": next_id()}},
            {"Type": "ObjectChipComponent", "Data": {
                "mId": next_id()}},
        ],
        "Children": children}}}
    out = f"{PREFABS}/Obj{name.capitalize()}.proto"
    json.dump(proto, open(out, "w"), indent=4)
    print(f"written {out} ({len(circles)} circles)")


if __name__ == "__main__":
    for i, n in enumerate(NAMES):
        make(n, i)
