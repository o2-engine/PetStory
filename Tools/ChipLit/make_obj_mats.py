#!/usr/bin/env python3
"""Generates {name}_obj.mat for the free-form field objects (Assets/Game field/Objects):
normal-map delta lighting — the material carries the {name}_nrm.png sampler and one
strength param. Run BuildAssets once after objects_normal.py so the _nrm metas exist."""
import json, os, sys

SP = os.path.dirname(os.path.abspath(__file__))
OBJ = "/Users/andreizenkovich/work/PetStory/Assets/Game field/Objects"
CHIPLIT = "/Users/andreizenkovich/work/PetStory/Assets/ChipLit"
from objects_normal import NAMES, DIFF_K, K_OVERRIDE


def meta_id(path):
    return json.load(open(path))["Value"]["mId"]


def make(name):
    import json as _json
    sc = _json.load(open(f"{SP}/{name}_obj.json"))["shadowColor"]
    c = [int(round(min(max(float(x), 0), 1)*255)) for x in sc]
    mat = {
        "mParams": [
            {"Type": "o2::ShaderParamFloatVector",
             "Value": {"mName": "u_fx", "mValue": [K_OVERRIDE.get(name, DIFF_K), 0.0, 0.0, 0.0]}},
            {"Type": "o2::ShaderParamColor",
             "Value": {"mName": "u_shadowColor",
                       "mValue": {"r": c[0], "g": c[1], "b": c[2], "a": 255}}},
        ],
        "mSamplers": [
            {"samplerUniformName": "u_nrmMap",
             "texCoordsAttrName": "a_texCoords2",
             "image": {"id": meta_id(f"{OBJ}/{name}_nrm.png.meta"),
                       "path": f"Game field/Objects/{name}_nrm.png"}},
        ],
        "mVertexShaderAsset": {"id": meta_id(f"{CHIPLIT}/chip_sdf.vert.meta"),
                               "path": "ChipLit/chip_sdf.vert"},
        "mFragmentShaderAsset": {"id": meta_id(f"{OBJ}/object_lit.frag.meta"),
                                 "path": "Game field/Objects/object_lit.frag"},
    }
    out = f"{OBJ}/{name}_obj.mat"
    json.dump(mat, open(out, "w"), indent=4)
    print("written", out)


if __name__ == "__main__":
    for n in sys.argv[1:] or NAMES:
        make(n)
