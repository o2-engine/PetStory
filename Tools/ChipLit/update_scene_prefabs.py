#!/usr/bin/env python3
"""Wires the sdf materials into the chip prototypes and instantiates them in test.scn."""
import json, os, random

REPO = "/Users/andreizenkovich/work/PetStory"
MAIN = f"{REPO}/Assets/Game field/Objects/Main"
PREFABS = f"{REPO}/Assets/Prefabs"
COLORS = ["Red", "Blue", "Green", "Orange", "Violet", "Yellow"]


def meta_id(path):
    return json.load(open(path))["Value"]["mId"]


def update_prefabs():
    for color in COLORS:
        c = color.lower()
        path = f"{PREFABS}/Chip{color}.proto"
        proto = json.load(open(path))
        components = proto["mActor"]["Value"]["Components"]
        for comp in components:
            if comp["Type"] == "o2::ImageComponent":
                data = comp["Data"]
                data["mImageAsset"] = {"id": meta_id(f"{MAIN}/{c}_df.png.meta"),
                                       "path": f"Game field/Objects/Main/{c}_df.png"}
                data["mMaterialAsset"] = {"id": meta_id(f"{MAIN}/{c}_sdf.mat.meta"),
                                          "path": f"Game field/Objects/Main/{c}_sdf.mat"}
                data["mColor"] = {"r": 255, "g": 255, "b": 255, "a": 255}
        json.dump(proto, open(path, "w"), indent=4)
        print("prefab updated:", path)


def update_scene():
    scn_path = f"{REPO}/Assets/test.scn"
    scn = json.load(open(scn_path))
    # keep the camera, drop previously added chip instances (idempotent reruns)
    scn["Actors"] = [a for a in scn["Actors"]
                     if not (a.get("Value", {}).get("mName") or "").startswith("Chip")]
    rng = random.Random(20260719)
    positions = [(-350, 250), (0, 250), (350, 250), (-350, -250), (0, -250), (350, -250)]
    for color, (x, y) in zip(COLORS, positions):
        proto_path = f"{PREFABS}/Chip{color}.proto"
        proto = json.load(open(proto_path))
        root = proto["mActor"]["Value"]
        components = [{"Type": comp["Type"],
                       "Data": {"PrototypeLink": comp["Data"]["mId"],
                                "mId": rng.getrandbits(63)}}
                      for comp in root.get("Components", [])]
        scn["Actors"].append({
            "Type": "o2::RigidBody",
            "Value": {
                "Prototype": {"id": meta_id(proto_path + ".meta"),
                              "path": f"Prefabs/Chip{color}.proto"},
                "PrototypeLink": root["Id"],
                "Id": rng.getrandbits(63),
                "mName": f"Chip{color}",
                "Transform": {
                    "position": {"x": x, "y": y},
                    "size": {"x": 210.0, "y": 210.0},
                    "scale": {"x": 1.0, "y": 1.0},
                    "pivot": {"x": 0.5, "y": 0.5},
                },
                "Components": components,
            },
        })
    json.dump(scn, open(scn_path, "w"), indent=4)
    print(f"test.scn updated: {len(scn['Actors'])} actors")


if __name__ == "__main__":
    update_prefabs()
    update_scene()
