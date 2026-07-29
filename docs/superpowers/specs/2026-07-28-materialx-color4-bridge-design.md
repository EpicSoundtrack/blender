# MaterialX Color4 Image Bridge

## Goal

Preserve MaterialX image alpha through USDShade-to-Cycles lowering without inventing a general Cycles RGBA socket. The initial scope supports only `ND_image_color4`, `ND_extract_color4`, and `ND_convert_color4_color3`.

## Boundary

Add `Color4` to the MaterialX IR and retain a `float4` literal-input representation. A lowered Color4 value is only supported when produced by `ND_image_color4`; it is represented by one Cycles Image Texture node, whose `Color` output carries RGB and whose `Alpha` output carries alpha.

Unsupported arbitrary Color4 producers and links fail clearly. This avoids fabricated alpha values while keeping the first feature bounded.

## Reader and validation

- Require USD `Color4f` for Color4 outputs and literals.
- `ND_image_color4` accepts the existing supported image file/UV attributes.
- `ND_extract_color4` requires a literal integer index from 0 through 3.
- `ND_convert_color4_color3` intentionally drops alpha and emits RGB.
- Unsupported image layer, animation, missing-file, address, filter, and Color4 producer cases reject rather than silently degrade.

## Lowering

- `image_color4`: one Image Texture node with UV linked to `Vector`.
- `extract_color4` index 0–2: RGB channel from Separate Color.
- `extract_color4` index 3: Image Texture `Alpha` directly.
- `convert_color4_color3`: Image Texture `Color` directly.

## Tests

1. Structural image Color4 to RGB conversion and alpha extraction from one Image Texture node.
2. Indices 0–3 route correctly; invalid, linked, or missing indices reject.
3. USDShade `Color4f` end-to-end reader/lowering test with explicit unsupported-producer rejection.

## Non-goals

- No arbitrary Color4 graph algebra.
- No Color4 Cycles socket.
- No silent alpha fallback, premultiplication conversion, or unsupported image-control approximation.
