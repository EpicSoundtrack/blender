# Correction: batch 48 commit message (unauthored float image colorspace)

Applies to commit `4a667c0dfab39ca025b26e6eddfae345c422cf98`
("Cycles MaterialX: unauthored float image colorspace is raw data",
branch `fix/trackb-float-image-raw-20261121`, native batch 48).

That commit is not rewritten: host build evidence binds to its sha.
This note corrects its message.

## What the message says

> their lowerings never set a colorspace on the ImageTextureNodes they
> create, so Cycles kept its default "auto" and decoded 8-bit files as
> sRGB.

## What is correct

Before batch 48, the `ND_tiledimage_float` and
`ND_triplanarprojection_float` lowerings did not set a colorspace on the
`ImageTextureNode`s they created. That part is right. The node then kept
the colorspace set by its constructor, which is `scene_linear`, not
`"auto"`:

```cpp
// intern/cycles/scene/shader_nodes.cpp:296-299 (same at 4a667c0d and at this line's tip)
ImageTextureNode::ImageTextureNode() : ImageSlotTextureNode(get_node_type())
{
  colorspace = u_colorspace_scene_linear;
```

Only `ND_image_color3` and the `hextiledimage` colour lowerings pass an
empty colorspace through `image_color3_colorspace()`, which maps it to
`u_colorspace_auto`. The float branches did not call it. Neither did the
`tiledimage` and `triplanarprojection` colour branches, which also kept
`scene_linear`.

So, before batch 48, 8-bit files read through these two float nodes were
**not** decoded as sRGB. After batch 48 they lower to Cycles `data`
(raw). Authored `raw`, `lin_rec709` and `srgb_texture` are honoured, and
any other authored colorspace is refused.

## Consequences

- For unauthored float images, batch 48 does not change the values that
  were measured. Lane b70x (2026-11-21) re-certified `ND_tiledimage_float`
  and `ND_constant_filename` on the batch-48 host with no change in
  values. That explains why both had passed raw expectations on earlier
  hosts. This is a measured result for those certified packets, not a
  proof for every possible input.
- The rest of batch 48 stands: reading the authored file colorspace,
  refusing unsupported values, and the test that pins unauthored to raw.
- The batch-48 host receipt
  (`scratch/native-windows-lowering-batch48-20261121/build.receipt.json`)
  repeats the "auto" wording in its "before" fields and its
  mutation-check text. It is retained evidence and is not edited. Read it
  with this correction.
- The batch-48 test comment in
  `intern/cycles/test/materialx_usdshade_reader_test.cpp` (line 52380 at
  `3be8adbf`) repeats the "auto" wording. Read it with this correction.
  It is left unchanged here so that this correction stays
  documentation-only.
- The value difference left between `scene_linear` and `data` is alpha
  handling. `scene_linear` is not data, so unassociated alpha in RGBA
  files may be treated differently.

Sources: the `shader_nodes.cpp` constructor at `4a667c0d` and the
release-line tip; `intern/cycles/materialx/graph.cpp` at `ce44a5558b31`
(the parent of batch 48), where the float branches have no
`set_colorspace` call; and the project audit log
`docs/REDTEAM-AUDIT-2026-11-10.md`, 2026-11-21 08:59 entry (b70x).
