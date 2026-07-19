#!/usr/bin/env python3
"""Generates red_sdf.mat / blue_sdf.mat from the fitted sdf params."""
import json, os

SP = os.path.dirname(os.path.abspath(__file__))
ASSETS = "/Users/andreizenkovich/work/PetStory/Assets/ChipLit"
TEX, DF_RANGE = 420.0, 128.0

VEC2 = ["lightDir", "bounceDir", "glintDir", "shadowDir", "center"]
COLORS = ["baseTop", "baseBottom", "darkColor", "litColor", "bounceColor", "bounceLineColor",
          "shadowColor", "glintColor", "iconTop", "iconBottom", "iconLitColor",
          "iconShadeColor", "iconBounceColor", "iconGlintColor"]


def meta_id(path):
    return json.load(open(path))["Value"]["mId"]


def make(name):
    p = json.load(open(f"{SP}/{name}_sdf_params.json"))
    dfmeta = json.load(open(f"{SP}/{name}_df.json"))
    p = dict(p)
    # engine texCoords are y-up: the center's v coordinate flips
    p["center"] = [dfmeta["cx"], 1.0 - dfmeta["cy"]]
    p["radiusUV"] = dfmeta["R"]
    p["texSize"] = TEX
    p["dfRange"] = DF_RANGE
    p["alphaOff"] = dfmeta.get("alphaOff", 0.0)
    p["alphaSoft"] = dfmeta.get("alphaSoft", 1.5)
    p["iconAA"] = p.get("iconAA", 2.5)

    params = []
    for k in VEC2:
        params.append({"Type": "o2::ShaderParamVec2",
                       "Value": {"mName": "u_" + k,
                                 "mValue": {"x": round(float(p[k][0]), 5), "y": round(float(p[k][1]), 5)}}})
    for k in COLORS:
        c = [int(round(min(max(float(x), 0), 1)*255)) for x in p[k]]
        params.append({"Type": "o2::ShaderParamColor",
                       "Value": {"mName": "u_" + k,
                                 "mValue": {"r": c[0], "g": c[1], "b": c[2], "a": 255}}})
    dots = (p.get("dots", []) + [[0, 1, 0, 1, 0]]*4)[:4]
    params.append({"Type": "o2::ShaderParamFloatVector",
                   "Value": {"mName": "u_dots",
                             "mValue": [round(float(x), 4) for d in dots for x in d[:4]]}})
    params.append({"Type": "o2::ShaderParamFloatVector",
                   "Value": {"mName": "u_dotStrengths",
                             "mValue": [round(float(d[4]), 4) for d in dots]}})
    c = [int(round(min(max(float(x), 0), 1)*255)) for x in p.get("dotColor", [1, 1, 1])]
    params.append({"Type": "o2::ShaderParamColor",
                   "Value": {"mName": "u_dotColor",
                             "mValue": {"r": c[0], "g": c[1], "b": c[2], "a": 255}}})

    skip = set(VEC2) | set(COLORS) | {"glintPos", "dots", "dotColor"}
    for k, v in p.items():
        if k in skip or isinstance(v, list):
            continue
        params.append({"Type": "o2::ShaderParamFloat",
                       "Value": {"mName": "u_" + k, "mValue": round(float(v), 5)}})

    mat = {
        "mParams": params,
        "mSamplers": [{
            "samplerUniformName": "u_lutMap",
            "texCoordsAttrName": "a_texCoords2",
            "image": {"id": meta_id(f"{ASSETS}/{name}_lut.png.meta"), "path": f"ChipLit/{name}_lut.png"},
        }],
        "mVertexShaderAsset": {"id": meta_id(f"{ASSETS}/chip_sdf.vert.meta"), "path": "ChipLit/chip_sdf.vert"},
        "mFragmentShaderAsset": {"id": meta_id(f"{ASSETS}/chip_sdf.frag.meta"), "path": "ChipLit/chip_sdf.frag"},
    }
    out = f"{ASSETS}/{name}_sdf.mat"
    json.dump(mat, open(out, "w"), indent=4)
    print("written", out, f"({len(params)} params)")


if __name__ == "__main__":
    make("red")
    make("blue")
