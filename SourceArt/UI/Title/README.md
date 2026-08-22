# Title Menu Button Source Art

Generated on 2026-08-22 for Project Museum Heist with the built-in OpenAI `imagegen` tool.

## Runtime intent

- `T_TitleButton_Normal.png`: shared Normal brush for Title menu buttons.
- `T_TitleButton_Hovered.png`: shared Hovered brush for Title menu buttons.
- `T_TitleButton_Pressed.png`: shared Pressed brush for Title menu buttons.
- `T_TitleLogo_Emblem.png`: text-free museum arch and keyhole emblem used beside the editable Title text.
- Button labels and icons remain separate UMG children. No language text is baked into these textures.

## Generation prompts

All three prompts requested a centered, front-facing 4:1 pill-shaped UMG button background with a transparent alpha canvas, dark charcoal/espresso fill, antique-gold border, no text, no icon, no logo and no watermark.

- Normal: calm inactive state with subtle smoked-glass depth and restrained warm highlights.
- Hovered: brighter antique-gold edge and controlled amber inner glow while retaining a dark center for white Korean text.
- Pressed: darker center, reduced top highlight, top inner shadow and compact lower-edge highlight to read as depressed.
- Logo emblem: centered circular museum arch and keyhole medallion with antique-gold metal and no letters or existing brand mark.

## Local validation

- All four delivered PNGs are RGBA.
- Alpha extrema are `0..255`, confirming transparent and opaque pixels are both present.
- The first edit-based Hovered/Pressed drafts were rejected because their checkerboard backgrounds were baked RGB pixels; they are not stored in this repository.
- `build_title_widgets.py` imports the textures and creates/updates the three Title Widget Blueprints through Unreal's UMGToolSet without EventGraph logic.
- `verify_title_widgets.py` checks native parents, required `BindWidget` variables, Blueprint compilation, and the four main buttons' Normal/Hovered/Pressed texture assignments.

## Provenance

These files were generated specifically for this project without an external reference image. This record documents local provenance; it is not a legal opinion and does not replace final release review.
