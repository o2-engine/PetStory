#!/usr/bin/env python3
"""Generates the game window UI assets from the PSD mockups.

Composites the popup sprites from AssetsSources/UI/*.psd into Assets/UI/ and
writes the window prefabs (SettingsWindow / WinWindow / BuyMovesWindow .proto)
as o2 widget hierarchies: Buttons, Images and Labels laid out with anchors and
offsets taken straight from the PSD coordinates (canvas 2160x3840, o2 world =
canvas center origin, y up).

All texts are real labels: each carries a LocalizedTextComponent with a key
from Assets/Localization/<lang>.json. Button press and toggle switch
animations are shared AnimationAsset files (UI/*.anim) referenced by the
widget states. Window logic scripts are referenced from the prefabs as
ScriptableComponent (Assets/Scripts/UI/*.js).

Run from the repo root: python3 Tools/UIGen/generate_ui.py
Re-run after changing the PSD sources or the layout tables below.
"""

import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, "o2", "Tools", "PsdTool"))

import psd_lib  # noqa: E402
from psd_to_o2 import new_id, new_uid, read_asset_id, write_meta  # noqa: E402

from PIL import Image  # noqa: E402

ASSETS = os.path.join(REPO, "Assets")
OUT_DIR = "UI"
SOURCES = os.path.join(REPO, "AssetsSources", "UI")
CANVAS = (2160, 3840)

FONT_META = os.path.join(ASSETS, "Fonts", "GrilledCheese BTN.ttf.meta")
TEXT_BROWN = {"r": 122, "g": 82, "b": 55, "a": 255}

STYLE_PLAIN = "UI/TextPlain.fntstyle"
STYLE_SHADOW = "UI/TextShadow.fntstyle"

ANIM_PRESSED = "UI/ButtonPressed.anim"
ANIM_HOVER = "UI/ButtonHover.anim"
ANIM_TOGGLE = "UI/ToggleValue.anim"


# ----------------------------------------------------------------------------
# Sprite compositing
# ----------------------------------------------------------------------------

def find_layer(root, path):
    parts = path.split("/")
    node = root
    for part in parts:
        found = None
        for child in node:
            if psd_lib.layer_name(child).strip() == part:
                found = child
                break
        if found is None:
            raise KeyError(f"layer '{path}' not found (missing '{part}')")
        node = found
    return node


def compose(psd, layer_paths):
    """Composites the listed layers (bottom to top) into one RGBA image."""
    layers = [find_layer(psd, path) for path in layer_paths]

    boxes = [layer.bbox for layer in layers]
    left = min(box[0] for box in boxes)
    top = min(box[1] for box in boxes)
    right = max(box[2] for box in boxes)
    bottom = max(box[3] for box in boxes)

    canvas = Image.new("RGBA", (right - left, bottom - top), (0, 0, 0, 0))
    for layer in layers:
        layer.visible = True  # some sources keep alternate states hidden
        image = layer.composite()
        if image is None:
            continue
        image = image.convert("RGBA")
        if layer.opacity != 255:
            alpha = image.getchannel("A").point(lambda v: v * layer.opacity // 255)
            image.putalpha(alpha)
        canvas.alpha_composite(image, (layer.bbox[0] - left, layer.bbox[1] - top))

    return canvas, (left, top, right, bottom)


# (out name, source psd, layer paths bottom-to-top); bbox = union of the layers
SPRITES = {
    "win_pop-up_fin.psd": [
        ("PanelPlank", ["pop-up/panel/bg",
                        "pop-up/panel/paper/shadow", "pop-up/panel/paper/paper",
                        "pop-up/panel/paper/pins/shadow", "pop-up/panel/paper/pins/pins"]),
        ("GreenButtonBg", ["pop-up/ok/bg", "pop-up/ok/shadow"]),
        ("GreenButton", ["pop-up/ok/button"]),
        ("CloseButtonBg", ["pop-up/x/bg", "pop-up/x/shadow"]),
        ("CloseButton", ["pop-up/x/button"]),
        ("StarGold", ["stars/2"]),
        ("StarBlue", ["stars/2a"]),
        ("DogPeek", ["dog"]),
    ],
    "MOVES_fin.psd": [
        ("MovesBubble", ["bubble/bubble"]),
        ("CoinSmall", ["pop-up/panel/coin"]),
    ],
    "settings_fin.psd": [
        ("SettingsPanel", ["Settings/plank+paper/plank", "Settings/plank+paper/shadow",
                           "Settings/plank+paper/paper",
                           "Settings/plank+paper/pins/shadow", "Settings/plank+paper/pins/pins"]),
        ("SettingsHead", ["Settings/head/shadow", "Settings/head/head", "Settings/head/nails"]),
        ("RowBox", ["Settings/Privacy policy/box"]),
        ("IconButtonBg", ["Settings/Privacy policy/button/bg", "Settings/Privacy policy/button/shadow"]),
        ("IconShield", ["Settings/Privacy policy/button/button"]),
        ("IconDoc", ["Settings/Terms of use/button/button"]),
        ("IconFb", ["Settings/Connect to/button/button"]),
        ("ToggleOn", ["Settings/tumbler/on/bg"]),
        ("ToggleOff", ["Settings/tumbler/off/bg"]),
        ("ToggleKnob", ["Settings/tumbler/on/tumbler"]),
        ("IconSound", ["Settings/tumbler/icon_sound"]),
        ("IconMusic", ["Settings/tumbler/icon_music"]),
    ],
}

# Sprites of the former baked texts, replaced by localized labels
OBSOLETE_SPRITES = ["CaptionNext", "CaptionBuy", "CaptionOk", "WinTitle",
                    "MovesBubbleText", "BuyMovesText",
                    "TextPrivacy", "TextTerms", "TextConnect"]


def o2_pos(bbox):
    """PSD bbox -> o2 world center of that bbox."""
    cx = (bbox[0] + bbox[2]) * 0.5
    cy = (bbox[1] + bbox[3]) * 0.5
    return (cx - CANVAS[0] * 0.5, CANVAS[1] * 0.5 - cy)


def generate_sprites(atlas_id):
    out_abs = os.path.join(ASSETS, OUT_DIR)
    os.makedirs(out_abs, exist_ok=True)

    for name in OBSOLETE_SPRITES:
        for suffix in (".png", ".png.meta"):
            path = os.path.join(out_abs, name + suffix)
            if os.path.exists(path):
                os.remove(path)

    for psd_name, specs in SPRITES.items():
        psd = psd_lib.load_psd(os.path.join(SOURCES, psd_name))
        for out_name, layer_paths in specs:
            image, bbox = compose(psd, layer_paths)
            path = os.path.join(out_abs, out_name + ".png")
            psd_lib.save_png(image, path)
            write_image_meta(path, atlas_id)
            print(f"  {out_name}.png {image.size} at o2{o2_pos(bbox)}")

    # Full-screen dimmer, tinted via the sprite color in the prefabs
    dim = Image.new("RGBA", (8, 8), (0, 0, 0, 255))
    dim_path = os.path.join(out_abs, "Dim.png")
    dim.save(dim_path)
    write_image_meta(dim_path, atlas_id)


def write_image_meta(png_path, atlas_id):
    if os.path.exists(png_path + ".meta"):
        return
    meta_value = {"mId": new_uid()}
    if atlas_id:
        meta_value["atlasId"] = atlas_id
    write_meta(png_path + ".meta", "o2::ImageAsset::Meta", meta_value)


# ----------------------------------------------------------------------------
# Shared animation and font style assets
# ----------------------------------------------------------------------------

def write_asset(rel_path, content, meta_type):
    path = os.path.join(ASSETS, rel_path)
    with open(path, "w") as f:
        json.dump(content, f, indent=4, ensure_ascii=False)
        f.write("\n")
    if not os.path.exists(path + ".meta"):
        write_meta(path + ".meta", meta_type, {"mId": new_uid()})
    print(f"  {rel_path}")


def curve_keys(duration, ease=0.4):
    return [
        {"uid": new_id(), "rightSupportPosition": round(duration * ease, 4),
         "supportsType": "Broken"},
        {"uid": new_id(), "value": 1.0, "position": duration,
         "leftSupportPosition": round(-duration * ease, 4), "supportsType": "Broken"},
    ]


def float_track(path, begin, end, duration):
    return {
        "Type": "o2::AnimationTrack<float>",
        "Value": {
            "path": path,
            "curve": {
                "Type": "o2::Curve",
                "Value": {
                    "mKeys": [
                        {"uid": new_id(), "value": begin,
                         "rightSupportPosition": round(duration * 0.4, 4),
                         "supportsType": "Broken"},
                        {"uid": new_id(), "value": end, "position": duration,
                         "leftSupportPosition": round(-duration * 0.4, 4),
                         "supportsType": "Broken"},
                    ]
                },
            },
        },
    }


def animation_asset(tracks, duration):
    return {
        "animation": {
            "Type": "o2::AnimationClip",
            "Value": {"mTracks": tracks, "mDuration": duration},
        }
    }


PRESS_SCALE = 0.92  # the whole button squeezes to this scale on press


def generate_shared_assets():
    # Button press: layer drawables and the caption text squeeze in. Scale
    # animates through the float scaleX/scaleY properties of the drawables:
    # widget transform scale is ignored by the layout-driven rendering
    duration = 0.06
    press_targets = ["layer/back/mDrawable", "layer/regular/mDrawable",
                     "child/Caption/layer/text/mDrawable"]
    press_tracks = []
    for target in press_targets:
        press_tracks.append(float_track(target + "/scaleX", 1.0, PRESS_SCALE, duration))
        press_tracks.append(float_track(target + "/scaleY", 1.0, PRESS_SCALE, duration))

    write_asset(ANIM_PRESSED, animation_asset(press_tracks, duration),
                "o2::DefaultAssetMeta<o2::AnimationAsset>")

    # Button hover: the face layer fades a little
    write_asset(ANIM_HOVER,
                animation_asset([float_track("layer/regular/transparency", 1.0, 0.85, 0.1)], 0.1),
                "o2::DefaultAssetMeta<o2::AnimationAsset>")

    # Toggle on: the knob slides right, backgrounds crossfade green over red
    write_asset(ANIM_TOGGLE,
                animation_asset([
                    float_track("child/Knob/layout/offsetLeft", -174.0, -4.0, 0.15),
                    float_track("child/Knob/layout/offsetRight", 4.0, 174.0, 0.15),
                    float_track("child/BgOn/transparency", 0.0, 1.0, 0.15),
                    float_track("child/BgOff/transparency", 1.0, 0.0, 0.15),
                ], 0.15),
                "o2::DefaultAssetMeta<o2::AnimationAsset>")

    # Font styles: plain brown texts and white captions with a soft shadow
    write_asset("UI/TextPlain.fntstyle", {"mEffects": []},
                "o2::DefaultAssetMeta<o2::FontStyleAsset>")
    write_asset("UI/TextShadow.fntstyle",
                {"mEffects": [{"Type": "o2::FontShadowEffect",
                               "Value": {"blurRadius": 5.0, "offset": {"x": 2, "y": 2},
                                         "color": {"r": 0, "g": 0, "b": 0, "a": 110}}}]},
                "o2::DefaultAssetMeta<o2::FontStyleAsset>")


# ----------------------------------------------------------------------------
# Prefab building blocks
# ----------------------------------------------------------------------------

def vec(x, y):
    return {"x": round(float(x), 1), "y": round(float(y), 1)}


def layout(pos, size, anchor=(0.5, 0.5), anchor_max=None):
    """WidgetLayout: pos/size in pixels around the anchor point of the parent."""
    a_min, a_max = anchor, anchor_max or anchor
    return {
        "anchorMin": vec(*a_min),
        "anchorMax": vec(*a_max),
        "offsetMin": vec(pos[0] - size[0] * 0.5, pos[1] - size[1] * 0.5),
        "offsetMax": vec(pos[0] + size[0] * 0.5, pos[1] + size[1] * 0.5),
    }


def stretch_layout(border=0.0):
    return {
        "anchorMin": vec(0, 0),
        "anchorMax": vec(1, 1),
        "offsetMin": vec(border, border),
        "offsetMax": vec(-border, -border),
    }


def sprite(image_name, color=None):
    value = {"mImageAsset": {"path": f"{OUT_DIR}/{image_name}.png"}}
    if color:
        value["mColor"] = color
    return {"Type": "o2::Sprite", "Value": value}


def ext_sprite(asset_path, color=None):
    value = {"mImageAsset": {"path": asset_path}}
    if color:
        value["mColor"] = color
    return {"Type": "o2::Sprite", "Value": value}


def widget_layer(name, drawable, lay=None, depth=0.0):
    if lay is None:
        lay = stretch_layout()
    value = {
        "name": name,
        "layout": {
            "mAnchorMin": lay["anchorMin"],
            "mAnchorMax": lay["anchorMax"],
            "mOffsetMin": lay["offsetMin"],
            "mOffsetMax": lay["offsetMax"],
        },
        "mDrawable": drawable,
        "mUID": new_id(),
    }
    if depth:
        value["mDepth"] = depth
    return {"Type": "o2::WidgetLayer", "Value": value}


def widget_state(name, anim_path, off_speed=1.0):
    return {"Type": "o2::WidgetState",
            "Value": {"name": name, "offStateAnimationSpeed": off_speed,
                      "mAnimation": {"path": anim_path}}}


def widget(w_type, name, transform, layers=None, children=None, components=None,
           states=None, enabled=True, transparency=None):
    value = {
        "Id": new_id(),
        "mName": name,
        "Transform": transform,
        "InternalWidgets": [],
        "Layers": layers or [],
        "States": states or [],
    }
    if children:
        value["Children"] = [{"Type": t, "Data": v} for t, v in children]
    if components:
        value["Components"] = [{"Type": t, "Data": v} for t, v in components]
    if not enabled:
        value["mEnabled"] = False
    if transparency is not None:
        value["mTransparency"] = transparency
    return value


def image_widget(name, image_name, pos, size, anchor=(0.5, 0.5), color=None,
                 enabled=True, external=None, children=None, transparency=None):
    drawable = ext_sprite(external, color) if external else sprite(image_name, color)
    return ("o2::Image", widget("o2::Image", name, layout(pos, size, anchor),
                                layers=[widget_layer("image", drawable)],
                                children=children, enabled=enabled,
                                transparency=transparency))


def label_widget(name, pos, size, height, font_id, key=None, text="",
                 style=None, color=None, anchor=(0.5, 0.5), wrap=False,
                 hor_align="Middle"):
    """Label with a real text; key adds LocalizedTextComponent with that key."""
    text_value = {
        "mText": text,
        "mFontAssetId": font_id,
        "mHeight": height,
        "mHorAlign": hor_align,
        "mVerAlign": "Middle",
    }
    if color:
        text_value["mColor"] = color
    if style:
        text_value["mFontStyleAsset"] = {"path": style}
    if wrap:
        text_value["mWordWrap"] = True

    text_drawable = {"Type": "o2::Text", "Value": text_value}

    components = None
    if key:
        components = [("LocalizedTextComponent", {"mKey": key, "mId": new_id()})]

    return ("o2::Label", widget("o2::Label", name, layout(pos, size, anchor),
                                layers=[widget_layer("text", text_drawable)],
                                components=components))


def framed_button(name, pos, size, back, face, face_size, caption=None,
                  anchor=(0.5, 0.5)):
    """Button: wooden frame back, pressable face, shared press/hover states.

    The face layer stretches with pixel insets so the shared press animation
    can squeeze it through the layer anchors on any button size.
    caption: (key, height, label_size, offset, font_id) for a localized label child.
    """
    inset_x = (size[0] - face_size[0]) * 0.5
    inset_y = (size[1] - face_size[1]) * 0.5
    face_layout = {
        "anchorMin": vec(0, 0),
        "anchorMax": vec(1, 1),
        "offsetMin": vec(inset_x, inset_y),
        "offsetMax": vec(-inset_x, -inset_y),
    }

    layers = [
        widget_layer("back", sprite(back), stretch_layout()),
        widget_layer("regular", sprite(face), face_layout, depth=1.0),
    ]

    children = []
    if caption:
        key, height, label_size, offset, font_id = caption
        children.append(label_widget("Caption", offset, label_size, height, font_id,
                                     key=key, style=STYLE_SHADOW))

    states = [
        widget_state("hover", ANIM_HOVER, off_speed=0.25),
        widget_state("pressed", ANIM_PRESSED, off_speed=0.5),
    ]

    # The press animation scales the button around its center
    transform = layout(pos, size, anchor)
    transform["pivot"] = vec(0.5, 0.5)

    return ("o2::Button", widget("o2::Button", name, transform,
                                 layers=layers, children=children, states=states))


def dim_button():
    """Full-screen dark overlay; a Button so it swallows cursor input below."""
    color = {"r": 255, "g": 255, "b": 255, "a": 215}
    return ("o2::Button", widget("o2::Button", "Dim", stretch_layout(),
                                 layers=[widget_layer("back", sprite("Dim", color))]))


def script_component(script_name):
    return ("o2::ScriptableComponent",
            {"mScript": {"path": f"Scripts/UI/{script_name}.js"}, "mId": new_id()})


def window_root(name, children, script_name):
    root = widget("o2::Widget", name,
                  layout((0, 0), CANVAS),
                  children=children,
                  components=[script_component(script_name)])
    return {"mActor": {"Type": "o2::Widget", "Value": root}}


def write_proto(name, data):
    path = os.path.join(ASSETS, OUT_DIR, name + ".proto")
    with open(path, "w") as f:
        json.dump(data, f, indent=4, ensure_ascii=False)
        f.write("\n")
    if not os.path.exists(path + ".meta"):
        write_meta(path + ".meta", "o2::DefaultAssetMeta<o2::ActorAsset>", {"mId": new_uid()})
    print(f"  {OUT_DIR}/{name}.proto")


# ----------------------------------------------------------------------------
# Windows (all coordinates: o2 world of the 2160x3840 canvas, from the PSDs)
# ----------------------------------------------------------------------------

def rel(pos, parent_pos):
    return (pos[0] - parent_pos[0], pos[1] - parent_pos[1])


def build_win_window(font_id):
    panel_pos, panel_size = (-15, -548), (1914, 785)

    # NextButton hangs off the panel bottom edge: anchored to it, offsets from it
    next_rel = rel((28, -914), panel_pos)
    next_from_bottom = (next_rel[0], next_rel[1] + panel_size[1] * 0.5)

    panel_children = [
        image_widget("PanelBack", "PanelPlank", (0, 0), panel_size),
        label_widget("Message", rel((-20, -524), panel_pos), (1500, 405), 84, font_id,
                     key="win.message", style=STYLE_PLAIN, color=TEXT_BROWN, wrap=True),
        framed_button("NextButton", next_from_bottom, (521, 325),
                      "GreenButtonBg", "GreenButton", (451, 261),
                      caption=("win.next", 80, (470, 140), (-8, 4), font_id),
                      anchor=(0.5, 0.0)),
    ]

    stars = []
    gold_pos = [(-410, 882), (0, 976), (410, 882)]
    blue_pos = [(-397, 890), (13, 984), (423, 890)]
    for i in range(3):
        stars.append(image_widget(f"StarGold{i + 1}", "StarGold",
                                  rel(gold_pos[i], (0, 1920)), (1115, 1105), (0.5, 1.0)))
        stars.append(image_widget(f"StarBlue{i + 1}", "StarBlue",
                                  rel(blue_pos[i], (0, 1920)), (504, 425), (0.5, 1.0),
                                  enabled=False))

    children = [
        dim_button(),
        label_widget("Title", rel((44, 1344), (0, 1920)), (1900, 240), 150, font_id,
                     key="win.title", anchor=(0.5, 1.0)),
        *stars,
        image_widget("Dog", "DogPeek", (87, 70), (746, 1149)),
        ("o2::Widget", widget("o2::Widget", "Panel", layout(panel_pos, panel_size),
                              children=panel_children)),
        framed_button("CloseButton", (881, -192), (258, 252),
                      "CloseButtonBg", "CloseButton", (208, 204)),
    ]
    return window_root("WinWindow", children, "WinWindow")


def build_buy_moves_window(font_id):
    panel_pos, panel_size = (-15, -380), (1914, 785)

    panel_children = [
        image_widget("PanelBack", "PanelPlank", (0, 0), panel_size),
        label_widget("OfferLabel", rel((-101, -384), panel_pos), (1400, 175), 84, font_id,
                     style=STYLE_PLAIN, color=TEXT_BROWN),
        image_widget("Coin", "CoinSmall", rel((670, -375), panel_pos), (157, 156)),
        framed_button("BuyButton", rel((28, -748), panel_pos), (521, 325),
                      "GreenButtonBg", "GreenButton", (451, 261),
                      caption=("buyMoves.buy", 80, (470, 140), (0, 4), font_id)),
    ]

    bubble_pos = (372, 498)
    bubble_text = label_widget("BubbleText", rel((373, 520), bubble_pos), (860, 300),
                               76, font_id, key="buyMoves.bubble", style=STYLE_PLAIN,
                               color=TEXT_BROWN, wrap=True)
    bubble = ("o2::Image", widget("o2::Image", "Bubble", layout(bubble_pos, (1113, 690)),
                                  layers=[widget_layer("image", sprite("MovesBubble"))],
                                  children=[bubble_text]))

    coins_hud_children = [
        image_widget("CoinsBack", "CoinsBg", (-456, -192), (657, 184),
                     (1.0, 1.0), external="Animal screen/CoinsBg.png"),
        image_widget("CoinIcon", "Coin", (-202, -186), (220, 217),
                     (1.0, 1.0), external="Animal screen/Coin.png"),
        label_widget("CoinsLabel", (-475, -194), (330, 100), 64, font_id, text="0",
                     anchor=(1.0, 1.0)),
    ]

    children = [
        dim_button(),
        image_widget("Dog", "DogPeek", (-468, 236), (746, 1149)),
        bubble,
        ("o2::Widget", widget("o2::Widget", "Panel", layout(panel_pos, panel_size),
                              children=panel_children)),
        framed_button("CloseButton", (881, -24), (258, 252),
                      "CloseButtonBg", "CloseButton", (208, 204)),
        ("o2::Widget", widget("o2::Widget", "CoinsHud", stretch_layout(),
                              children=coins_hud_children)),
    ]
    return window_root("BuyMovesWindow", children, "BuyMovesWindow")


def toggle_widget(name, pos, parent_pos, font_id):
    """Tumbler: knob slides and backgrounds crossfade through the "value" state."""
    on_caption = label_widget("Caption", (-38, -4), (200, 100), 52, font_id,
                              key="common.on", style=STYLE_SHADOW)
    off_caption = label_widget("Caption", (36, -4), (200, 100), 52, font_id,
                               key="common.off", style=STYLE_SHADOW)

    children = [
        image_widget("BgOn", "ToggleOn", (-42, -1), (265, 140),
                     children=[on_caption], transparency=0.0),
        image_widget("BgOff", "ToggleOff", (42, -1), (265, 140),
                     children=[off_caption]),
        image_widget("Knob", "ToggleKnob", (-85, 2), (178, 177)),
    ]

    states = [widget_state("value", ANIM_TOGGLE)]

    return ("o2::Button", widget("o2::Button", name,
                                 layout(rel(pos, parent_pos), (350, 180)),
                                 children=children, states=states))


def build_settings_window(font_id):
    panel_pos, panel_size = (8.5, -96.5), (1675, 1983)

    def prel(pos):
        return rel(pos, panel_pos)

    rows = []
    for row, box_pos, key, face, btn_pos in [
        ("Privacy", (-72, 111), "settings.privacy", "IconShield", (466, 126)),
        ("Terms", (-24, -241), "settings.terms", "IconDoc", (466, -226)),
        ("Connect", (-12, -594), "settings.connect", "IconFb", (466, -580)),
    ]:
        box = prel(box_pos)
        rows.append(image_widget(f"{row}Box", "RowBox", box, (1117, 260)))

        # The text runs from the box left edge up to the row button
        box_left = box[0] - 1117 * 0.5
        button_left = prel(btn_pos)[0] - 338 * 0.5
        text_rect = (box_left + 60, button_left - 20)
        text_center = ((text_rect[0] + text_rect[1]) * 0.5, box[1] - 4)
        rows.append(label_widget(f"{row}Text", text_center,
                                 (text_rect[1] - text_rect[0], 140),
                                 58, font_id, key=key, style=STYLE_PLAIN,
                                 color=TEXT_BROWN, hor_align="Left"))
        rows.append(framed_button(f"{row}Button", prel(btn_pos), (338, 317),
                                  "IconButtonBg", face, (273, 250)))

    head_pos = prel((15, 895))

    panel_children = [
        image_widget("PanelBack", "SettingsPanel", (0, 0), panel_size),
        image_widget("Head", "SettingsHead", head_pos, (1068, 314)),
        label_widget("HeadTitle", (head_pos[0] + 9, head_pos[1] + 22), (900, 150),
                     96, font_id, key="settings.title", style=STYLE_SHADOW),
        image_widget("SoundIcon", "IconSound", prel((-481, 428)), (190, 189)),
        image_widget("MusicIcon", "IconMusic", prel((136, 428)), (165, 209)),
        toggle_widget("SoundToggle", (-170, 430), panel_pos, font_id),
        toggle_widget("MusicToggle", (410, 430), panel_pos, font_id),
        *rows,
        framed_button("OkButton", prel((28, -984)), (521, 325),
                      "GreenButtonBg", "GreenButton", (451, 261),
                      caption=("settings.ok", 80, (470, 140), (-16, 6), font_id)),
        framed_button("CloseButton", prel((780, 828)), (258, 252),
                      "CloseButtonBg", "CloseButton", (208, 204)),
    ]

    children = [
        dim_button(),
        ("o2::Widget", widget("o2::Widget", "Panel", layout(panel_pos, panel_size),
                              children=panel_children)),
    ]
    return window_root("SettingsWindow", children, "SettingsWindow")


def main():
    atlas_id = read_asset_id(os.path.join(ASSETS, "Basic.atlas.meta"))
    font_id = read_asset_id(FONT_META)
    if not font_id:
        raise SystemExit("font meta not found: " + FONT_META)

    print("Sprites:")
    generate_sprites(atlas_id)

    print("Shared assets:")
    generate_shared_assets()

    if not os.path.exists(os.path.join(ASSETS, OUT_DIR + ".meta")):
        write_meta(os.path.join(ASSETS, OUT_DIR + ".meta"),
                   "o2::DefaultAssetMeta<o2::FolderAsset>", {"mId": new_uid()})

    print("Prefabs:")
    write_proto("WinWindow", build_win_window(font_id))
    write_proto("BuyMovesWindow", build_buy_moves_window(font_id))
    write_proto("SettingsWindow", build_settings_window(font_id))


if __name__ == "__main__":
    main()
