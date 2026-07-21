#!/usr/bin/env python3
"""Trims BuiltAssets/WebAssembly/Data to the transitive closure of test.scn.

Walks asset references (scn -> prototypes -> materials -> images/shaders),
resolves atlas membership for packed images, keeps FrameworkData untouched.
The original Data is preserved as Data.full for easy restore.
"""
import json, os, shutil, sys, glob

REPO = "/Users/andreizenkovich/work/PetStory"
ASSETS = f"{REPO}/Assets"
WA = f"{REPO}/BuiltAssets/WebAssembly"
DATA = f"{WA}/Data"

EXTRA_KEEP = ["test.scn", "debugFont.ttf"]  # scene itself + debug font for overlays


def collect_refs(path, refs):
    """Recursive walk of a JSON asset for {'id','path'} references."""
    try:
        doc = json.load(open(path))
    except Exception:
        return
    def walk(node):
        if isinstance(node, dict):
            p = node.get("path")
            if p and "id" in node and isinstance(p, str) and p not in refs:
                refs.add(p)
                src = os.path.join(ASSETS, p)
                if os.path.exists(src) and p.endswith((".scn", ".proto", ".mat")):
                    collect_refs(src, refs)
        if isinstance(node, dict):
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for x in node:
                walk(x)
    walk(doc)


def atlas_map():
    """atlas meta id -> atlas asset path (relative to Assets)."""
    out = {}
    for meta in glob.glob(f"{ASSETS}/**/*.atlas.meta", recursive=True):
        try:
            mid = json.load(open(meta))["Value"]["mId"]
            out[mid] = os.path.relpath(meta[:-5], ASSETS)
        except Exception:
            pass
    return out


def main():
    refs = set(EXTRA_KEEP)
    collect_refs(f"{ASSETS}/test.scn", refs)

    # atlas closure for packed images
    atlases = atlas_map()
    used_atlases = set()
    for p in list(refs):
        meta_path = os.path.join(ASSETS, p + ".meta")
        if p.lower().endswith(".png") and os.path.exists(meta_path):
            try:
                aid = json.load(open(meta_path))["Value"].get("atlasId")
            except Exception:
                aid = None
            if aid and aid in atlases:
                used_atlases.add(atlases[aid])
    refs |= used_atlases

    # copy closure from Data into Data.trim (atlas built pages go by wildcard)
    trim = f"{WA}/Data.trim"
    if os.path.exists(trim):
        shutil.rmtree(trim)
    os.makedirs(trim)
    copied, missing = 0, []
    def copy_rel(rel):
        nonlocal copied
        src = os.path.join(DATA, rel)
        if not os.path.exists(src):
            missing.append(rel)
            return
        dst = os.path.join(trim, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(src, dst)
        copied += 1
    for rel in sorted(refs):
        copy_rel(rel)
        if rel.endswith(".atlas"):
            base = os.path.join(DATA, rel)
            for page in glob.glob(base + "*.png"):
                copy_rel(os.path.relpath(page, DATA))

    # swap: keep the full build for restore
    full = f"{WA}/Data.full"
    if os.path.exists(full):
        shutil.rmtree(full)
    os.rename(DATA, full)
    os.rename(trim, DATA)

    print(f"kept {copied} files")
    for rel in sorted(refs):
        print("  ", rel)
    if missing:
        print("MISSING in built data:", missing)
    os.system(f'du -sh "{DATA}" "{full}"')


if __name__ == "__main__":
    main()
