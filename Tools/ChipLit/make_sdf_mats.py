#!/usr/bin/env python3
"""Generates {color}_sdf.mat for every chip color into Assets/Game field/Objects/Main.
Simplified param set: gradients, shadow, glint, dots; geometry constants live in the
shader. DF/LUT textures are expected in the same folder (copied there beforehand)."""
import json, os, sys

SP = os.path.dirname(os.path.abspath(__file__))
MAIN = "/Users/andreizenkovich/work/PetStory/Assets/Game field/Objects/Main"
CHIPLIT = "/Users/andreizenkovich/work/PetStory/Assets/ChipLit"
COLORS_ALL = ["red", "blue", "green", "orange", "violet", "yellow"]


def meta_id(path):
    return json.load(open(path))["Value"]["mId"]


def color_param(name, rgb):
    c = [int(round(min(max(float(x), 0), 1)*255)) for x in rgb]
    return {"Type": "o2::ShaderParamColor",
            "Value": {"mName": name, "mValue": {"r": c[0], "g": c[1], "b": c[2], "a": 255}}}


def vec_param(name, values):
    return {"Type": "o2::ShaderParamFloatVector",
            "Value": {"mName": name, "mValue": [round(float(x), 5) for x in values]}}


def make(name):
    p = json.load(open(f"{SP}/{name}_sdf_simple.json"))

    params = [
        vec_param("u_grad", [p["gradSpread"], p["gradPow"], p["iconGradSpread"], p["iconGradPow"]]),
        color_param("u_baseTop", p["baseTop"]),
        color_param("u_baseBottom", p["baseBottom"]),
        color_param("u_iconTop", p["iconTop"]),
        color_param("u_iconBottom", p["iconBottom"]),
        vec_param("u_shadowColor", list(p["shadowColor"][:3]) + [p["shadowStrength"]]),
        vec_param("u_glint", [p["glintDir"][0], p["glintDir"][1], p["glintRad"], p["glintPow"]]),
        vec_param("u_glintEx", [p["glintRx"], p["glintRy"], p["glintStrength"], 0.0]),
        color_param("u_glintColor", p["glintColor"]),
    ]

    dots = (p.get("dots", []) + [[0, 1, 0, 1, 0]]*4)[:4]
    params.append(vec_param("u_dots", [x for d in dots for x in d[:4]]))
    params.append(vec_param("u_dotStrengths", [d[4] for d in dots]))
    params.append(color_param("u_dotColor", p.get("dotColor", [1, 1, 1])))

    mat = {
        "mParams": params,
        "mSamplers": [{
            "samplerUniformName": "u_lutMap",
            "texCoordsAttrName": "a_texCoords2",
            "image": {"id": meta_id(f"{MAIN}/{name}_lut.png.meta"),
                      "path": f"Game field/Objects/Main/{name}_lut.png"},
        }],
        "mVertexShaderAsset": {"id": meta_id(f"{CHIPLIT}/chip_sdf.vert.meta"), "path": "ChipLit/chip_sdf.vert"},
        "mFragmentShaderAsset": {"id": meta_id(f"{CHIPLIT}/chip_sdf.frag.meta"), "path": "ChipLit/chip_sdf.frag"},
    }
    out = f"{MAIN}/{name}_sdf.mat"
    json.dump(mat, open(out, "w"), indent=4)
    print("written", out, f"({len(params)} params)")


if __name__ == "__main__":
    names = sys.argv[1:] or COLORS_ALL
    for n in names:
        make(n)
