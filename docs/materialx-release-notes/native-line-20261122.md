# Native MaterialX release line, 2026-11-22

Branch: `release/native-materialx-line-20261122`. Pushed on 2026-11-23 19:43 -0600 to our own fork `origin` (`git@github.com:EpicSoundtrack/blender.git`) at `ee5b29316847`, per the remote-tracking reflog (`git reflog show --date=iso refs/remotes/origin/release/native-materialx-line-20261122`: "update by push"). This line said "local, not pushed" until then. Nothing was sent to Blender upstream.

The branch starts at `3be8adbf2c96` (the portability fix). It adds
documentation-only commits on top: the batch-48 correction
(`batch48-colorspace-correction.md`), this inventory, and the
review-wording fixes to both.

The independent review is `release-line-review-20261122-3f9a`
(PASS_WITH_NOTES).

The line's base is `1fbe3fbc587b` (`fix/trackb-materialx-only-20261117`,
the batch-39 lineage tip).

## Chain check

Every link below was checked with `git merge-base --is-ancestor`, and
each commit's parent was compared with the commit before it. The line
from `1fbe3fbc587b` to `3be8adbf2c96` is linear: 26 commits and no
merges. No merge was needed to form the release line.

## Batch commits

The summaries come from the commit subjects and the project audit log
(`materialx-finish-20260917/docs/REDTEAM-AUDIT-2026-11-10.md`).

| Batch | Commit(s) | Branch | Summary |
|---|---|---|---|
| 40 | `ad18b2ddf419` | fix/trackb-det44-runtime-20261120 | Runtime connected-Matrix44 `ND_determinant_matrix44`. |
| 41 | `e5254e72e6fe`, `527f16f96ca8` | fix/trackb-divide-matrix33-runtime-20261120 | Runtime `ND_divide_matrix33` as `in1*inverse(in2)`. Connected `in` for `ND_transformmatrix_vector3M4`. |
| 42 | `eb778711b9d6` | fix/trackb-transformmatrix-v3m4-runtime-mat-20261120 | Runtime `mat` for `ND_transformmatrix_vector3M4` (`[x,y,z,1]*M`, no w divide). |
| 43 | `2fb41ca499e1` | fix/trackb-bump-vector3-20261120 | `ND_bump_vector3` connected scale. `heighttonormal`->`normalmap_float` uses the UV-tangent frame. |
| 44 | `c80db24cf0b7` | fix/trackb-geomcolor-color4-index-20261120 | Linked constant index on `geomcolor_color4`. |
| 45 | `845051cd9eeb` | fix/trackb-geomcolor-color4-absent-alpha-20261120 | An absent `geomcolor_color4` set reads the nodedef default (0,0,0,0). |
| 46 | `3c44caa5f4bc` | fix/trackb-geomprop-nodedef-default-fallback-20261120 | Four-component readers with no authored default fall back to the nodedef default. |
| 47 | `b0628f200d7e`, `ce44a5558b31` | fix/trackb-gooch-viewdir-20261121 | `gooch_shade` view direction points from the camera to the surface. Unauthored `hextilednormalmap` colorspace is raw. |
| 48 | `4a667c0dfab3` | fix/trackb-float-image-raw-20261121 | Unauthored `tiledimage_float` / `triplanarprojection_float` colorspace is raw; authored raw/srgb_texture/lin_rec709 are honoured and others refused. **The message's "auto" claim is wrong; see `batch48-colorspace-correction.md`.** |
| 49 | `c377ea0a2775`, `4775cfc41641` | fix/trackb-if-select-nan-20261121 | Float `if*` conditionals lower to a true select. The tests anchor values and cover the constant-predicate bypass. |
| 50 | `059759161874`, `a56225df217b`, `1c77f1bf4a6e`, `84bf445f62a9`, `96108b2275ed` | fix/trackb-gltf-sheen-20261122 | `gltf_pbr` sheen is lowered and the nodedef defaults are applied. Sheen with emission is refused here, then admitted in batch 55. The rest is review follow-ups. |
| 51 | `9d3ca2fa3a87`, `a419d51e64cf` | fix/trackb-hair-lama-20261122 | Chiang hair normal/roughness links and `lama_diffuse` energyCompensation, plus review follow-ups. |
| 52 | `3afd5859b1f2` | fix/trackb-lama-surface-align-20261122 | `lama_surface` uses the front material on both faces (ruling Q8). |
| 53 | `c2cd58ec175d` | fix/trackb-surfacematerial-align-20261122 | `surfacematerial` uses the front surface on both faces (ruling Q9/Q9b). |
| 54 | `bfeecc84b5db` | fix/trackb-b85x-defects-20261122 | b85x defects: chiang absorption link, gltf literal normals, `lama_diffuse` energyCompensation 0. |
| 55 | `4abe5a1d3a6f`, `98c6a000f2ec` | fix/trackb-q6-orennayar-20261122 | `gltf_pbr` sheen+emission is admitted (Q6), while sheen+clearcoat is still refused. The uncompensated Oren-Nayar approximation is pinned by a kernel envelope test (Q11). Comment follow-ups. |
| port | `3be8adbf2c96` | fix/trackb-portable-test-includes-20261122 | The tests use in-repo fixture copies in place of absolute Windows paths. The Linux build of `3be8adbf` compiled on Omnistation (rc 0). No Linux `cycles_test` result is recorded. |

## Certificates and host lineage

This section reads `materialx-finish-20260917/configs/materialx-full-catalog-node-ledger-v1.json`
(sha256 prefix `056aa775`) read-only. It lists every node whose current
`paired_input_contract_evidence` names a batch 40-55 host by its
directory, `build_hash`, exe sha prefix or "batch-NN host" wording.

Because this line is linear, a certificate on a batch-N host was made on
a build whose source is an ancestor of this line. Later batches can still
change the rendering of such a node. Re-certify only where the audit log
records a later behaviour change for that node.

| Host lineage | Nodes (current certificate evidence) |
|---|---|
| batch 40 (`ad18b2ddf419`) | ND_determinant_matrix44, ND_transformmatrix_vector2M3, ND_transformmatrix_vector3 |
| batch 41 (`527f16f96ca8`) | ND_divide_matrix33 (Cycles-only) |
| batch 42 (`eb778711b9d6`) | ND_transformmatrix_vector3M4 |
| batch 45 (`845051cd9eeb`) | ND_geomcolor_color4 |
| batch 46 (`3c44caa5f4bc`) | ND_geompropvalue_boolean, ND_glossiness_anisotropy |
| batch 47 (`ce44a5558b31`) | ND_facingratio_float, ND_gooch_shade |
| batch 48 (`4a667c0dfab3`), by build_hash | ND_atan2_vector3, ND_fractal3d_vector2, ND_normal_vector3, ND_roughness_anisotropy, ND_roughness_dual |
| batch 48, by host wording only | ND_tiledimage_float and ND_constant_filename (b70x re-certification on the batch-48 host), plus 24 nodes whose evidence text names the git-built batch-48 host: ND_UsdUVTexture, ND_UsdUVTexture_23, ND_bitangent_vector3, ND_displacement_float, ND_displacement_vector3, ND_dot_displacementshader, ND_dot_filename, ND_frame_float, ND_gltf_normalmap_vector3_1_0, ND_invertmatrix_matrix44, ND_noise2d_color3, ND_normalmap_float, ND_normalmap_vector2, ND_subsurface_bsdf, ND_surface_unlit, ND_tangent_vector3, ND_transformpoint_vector3, ND_transformvector_vector3, ND_triplanarprojection_color3/color4/float/vector2/vector3/vector4 |
| batch 48 and batch 49 (`4775cfc41641`) | ND_fractal3d_vector4FA, ND_standard_surface_to_gltf_pbr |
| batch 53 (`c2cd58ec175d`) | ND_surfacematerial (Cycles-only re-certification) |
| batch 54 (`bfeecc84b5db`) | ND_chiang_hair_bsdf (Cycles-only, Q12 subset; its evidence says "batch-54 evidence (not batch 55)") |
| batch 55 (`98c6a000f2ec`) | ND_gltf_pbr_surfaceshader (Cycles-only, Q12 subset) |

No current certificate names a batch 43, 44, 50, 51 or 52 host, or the
portability commit.

The scan covers only names and identifiers that appear as text. A
certificate that cites none of them can still depend on this line.
Certificates on hosts older than batch 40 are not listed here.
