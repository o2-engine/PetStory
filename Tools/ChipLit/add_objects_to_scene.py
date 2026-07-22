#!/usr/bin/env python3
"""Wires the object-chip mechanics into Assets/test.scn: a container for the falling
objects, a spawn zone above the board, an ObjectsSpawnerComponent that releases them
one at a time, and a sensor-collider trigger at the field bottom that removes any
object chip reaching it (the spawner then refills from the top)."""
import json, os, sys

SP = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SP)
from objects_normal import NAMES
from make_obj_protos import meta_id

REPO = "/Users/andreizenkovich/work/PetStory"
PREFABS = f"{REPO}/Assets/Prefabs"
SCENE = f"{REPO}/Assets/test.scn"
BASE_ID = 77300000000000001

OURS = ("Objects", "FallingObjects", "ObjectsSpawner", "ObjSpawnZone", "ObjBottomTrigger")


def main():
    doc = json.load(open(SCENE))
    doc["Actors"] = [a for a in doc["Actors"]
                     if a.get("Value", {}).get("mName") not in OURS
                     and not str(a.get("Value", {}).get("mName", "")).startswith("Obj")]

    uid = BASE_ID
    def next_id():
        nonlocal uid
        uid += 1
        return uid

    container_id = next_id()
    zone_id = next_id()

    # container: objects live here, drawn above the chips
    doc["Actors"].append({"Type": "o2::Actor", "Value": {
        "Id": container_id,
        "mName": "FallingObjects",
        "Transform": {"position": {"x": 0.0, "y": 0.0},
                      "size": {"x": 10.0, "y": 10.0},
                      "scale": {"x": 1.0, "y": 1.0},
                      "pivot": {"x": 0.5, "y": 0.5}}}})

    # spawn zone: a strip above the visible board
    doc["Actors"].append({"Type": "o2::Actor", "Value": {
        "Id": zone_id,
        "mName": "ObjSpawnZone",
        "Transform": {"position": {"x": 0.0, "y": 740.0},
                      "size": {"x": 560.0, "y": 40.0},
                      "scale": {"x": 1.0, "y": 1.0},
                      "pivot": {"x": 0.5, "y": 0.5}}}})

    protos = [{"id": meta_id(f"{PREFABS}/Obj{n.capitalize()}.proto.meta"),
               "path": f"Prefabs/Obj{n.capitalize()}.proto"} for n in NAMES]
    doc["Actors"].append({"Type": "o2::Actor", "Value": {
        "Id": next_id(),
        "mName": "ObjectsSpawner",
        "Transform": {"position": {"x": 0.0, "y": 0.0},
                      "size": {"x": 10.0, "y": 10.0},
                      "scale": {"x": 1.0, "y": 1.0},
                      "pivot": {"x": 0.5, "y": 0.5}},
        "Components": [
            {"Type": "ObjectsSpawnerComponent", "Data": {
                "mSpawnDelay": 2.0,
                "mMaxCount": 3,
                "mSpawnContainer": {"ID": container_id},
                "mSpawnZone": {"ID": zone_id},
                "mObjectProtos": protos,
                "mId": next_id()}},
        ]}})

    # bottom trigger: a static body with a sensor box collider across the field bottom
    doc["Actors"].append({"Type": "o2::RigidBody", "Value": {
        "Id": next_id(),
        "mName": "ObjBottomTrigger",
        "mBodyType": "Static",
        "Transform": {"position": {"x": 0.0, "y": -600.0},
                      "size": {"x": 680.0, "y": 80.0},
                      "scale": {"x": 1.0, "y": 1.0},
                      "pivot": {"x": 0.5, "y": 0.5}},
        "Components": [
            {"Type": "o2::BoxCollider", "Data": {
                "mSize": {"x": 680.0, "y": 80.0},
                "mFitByActor": False,
                "mIsSensor": True,
                "mId": next_id()}},
            {"Type": "ObjectsBottomTriggerComponent", "Data": {
                "mObjectsContainer": {"ID": container_id},
                "mId": next_id()}},
        ]}})

    json.dump(doc, open(SCENE, "w"), indent=4, ensure_ascii=False)
    print("scene wired: container + spawn zone + spawner + bottom trigger")


if __name__ == "__main__":
    main()
