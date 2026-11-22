# SH2ZAM map

This map gives, for each sh2o operation, the SH2ZAM guide, the SH2ZAM kernel and the SH2ZAM model suite that are related to it. Read the row of an operation before you write its SH-2 kernel. [CONTRIBUTING-KERNELS.md](../CONTRIBUTING-KERNELS.md) gives the steps.

- **Commit:** the map was read at SH2ZAM `7f99eb70` (2026-11-20), which is the pin in `scripts/sh2zam_pin.txt`.
- **The map decides nothing.** It tells where to read. It does not tell which kernel to write, and it does not tell that SH2ZAM is linked. Those decisions are in the kernel rules and in the validation records.
- **Update:** read the map again when the pin moves, and change the commit, the line numbers and the rows in the commit that follows the pin change. Add a row when sh2o gets an operation that is not here.

## How to read a row

- The paths are in `external/SH2ZAM/`. A guide is given as `file:lines`. A kernel is given as `file:line` of its function. A model suite is a folder in `saturn/benchmarks/`.
- "none" means that no SH2ZAM guide, kernel or suite for the subject was found at this commit.
- The column "Related, not equal" tells why the SH2ZAM function does not give the bytes of the sh2o operation, or tells the test that proved it equal. SH2ZAM calculates in Q16.16, saturates and rounds down one time (`README.md:562-572`, `saturn/HARDWARE.md:17`). It has no GTE operation.
- The names of the operations that are not in sh2o yet are the names of the private port. They can change when the operation comes into sh2o.

## The methods

Four SH2ZAM methods are related to many rows. The rows give their short names.

| Method | Guide | Kernel | Model suite |
|---|---|---|---|
| MAC.L row: three `MAC.L` into MACH:MACL, then `XTRCT` takes bits 47 to 16 | `saturn/BATCH_TRANSFORMS.md:193-199`, `saturn/benchmarks/FUSED_SH2.md:61-65` | `saturn/src/sh2zam_batch_sh2.c:40` (`sh2zi_mac_row`) | `sh2-model` (its `README.md:52-58`) |
| SR.S, read and use a path without `MAC.L` | `saturn/BATCH_TRANSFORMS.md:201-206`, `saturn/benchmarks/FUSED_SH2.md:67-72`, `saturn/POSE.md:77-78` | `saturn/src/sh2zam_batch_sh2.c:17` (`sh2zi_mac_s_clear`) | `sh2-model` (its `README.md:57-58`), `pose-model` |
| SR.S, save, clear and restore | `saturn/UNIT30.md:63-65` | `saturn/include/sh2zam/unit30_experimental.h:128` (`sh2z_u30_dot3`) | `unit30-model` |
| Plan prepared at the load: the matrix is copied and its range is proven one time, and a call does no new range check of the data | `saturn/BATCH_TRANSFORMS.md:29-46`, `:48-58` (a part of the range) | `saturn/src/sh2zam_prepared.c:48` (`sh2z_transform_plan_init`), `:12` (`sh2z_mesh_view_init`) | `sh2-model` |

## In sh2o today: `fixed`

| Operation | SH2ZAM guide | SH2ZAM kernel | Model suite | Related, not equal |
|---|---|---|---|---|
| `sh2o_mul_q12` | `saturn/HARDWARE.md:17`, `README.md:562-572` | `saturn/src/sh2zam.c:27` (`sh2z_mul`) | `math-model` | `sh2z_mul` shifts by 16 and saturates. `sh2o_mul_q12` shifts by 12 and wraps. |
| `sh2o_lim_ir` | none | none | none | A GTE limit. |

## In sh2o today: `k`

| Operation | SH2ZAM guide | SH2ZAM kernel | Model suite | Related, not equal |
|---|---|---|---|---|
| `sh2o_k_dot3b_w` (entry `sh2o_k_dot3b`) | MAC.L row | `saturn/src/sh2zam_batch_sh2.c:40` | `sh2-model` | The sh2o kernel uses `MAC.W` on 16-bit values. SH2ZAM has no `MAC.W` kernel. |
| `sh2o_k_mvb3_w` | MAC.L row | `saturn/src/sh2zam_batch_sh2.c:40` | `sh2-model` | The same. |
| `sh2o_k_gpf3_w` (entry `sh2o_k_gpf3`) | MAC.L row | `saturn/src/sh2zam_batch_sh2.c:40` | `sh2-model` | The same. |
| `sh2o_k_gpl3_w` (entry `sh2o_k_gpl3`) | MAC.L row | `saturn/src/sh2zam_batch_sh2.c:40` | `sh2-model` | The same. |
| `sh2o_k_dot3b_w2` | SR.S, read and use a path without `MAC.L` | `saturn/src/sh2zam_batch_sh2.c:17` | `sh2-model` | SH2ZAM checks SR.S for `MAC.L`. The sh2o kernel checks it for `MAC.W`, which also uses SR.S. Not linked. |
| `sh2o_k_mvb3_w2` (entry `sh2o_k_mvb3`) | SR.S, read and use a path without `MAC.L` | `saturn/src/sh2zam_batch_sh2.c:17` | `sh2-model` | The same. |
| `sh2o_k_gpf3_w2` | SR.S, read and use a path without `MAC.L` | `saturn/src/sh2zam_batch_sh2.c:17` | `sh2-model` | The same. |
| `sh2o_k_gpl3_w2` | SR.S, read and use a path without `MAC.L` | `saturn/src/sh2zam_batch_sh2.c:17` | `sh2-model` | The same. |
| `sh2o_k_unr_div` | `saturn/MATH_OPTIMIZATIONS.md:79-90` (`DIV1` steps), `saturn/DIVU.md:1-12` (the divider unit) | `saturn/src/sh2zam.c:28` (`sh2z_div`), `saturn/include/sh2zam/divu_experimental.h:216` | `math-model`, `trig12-model` | SH2ZAM gives the exact quotient. The GTE divide is a table approximation, and sh2o must give its bytes. |
| `sh2o_k_unr_recip` | `saturn/MATH_OPTIMIZATIONS.md:79-90` | `saturn/src/sh2zam.c:56` (`sh2z_recip`) | `math-model` | The same. |
| `sh2o_k_clz32` | none | none | none | |

## In sh2o today: `gte`

All these operations are GTE-exact: they give the GTE results and FLAG bits. No SH2ZAM function is proven equal to one of them. On the SH-2, the MAC stage of `sh2o_gte_mvmva` (not mx = 3, not cv = 2), `sh2o_gte_rtps`, `sh2o_gte_rtpt`, `sh2o_gte_ncs`, `sh2o_gte_nct`, `sh2o_gte_ncds`, `sh2o_gte_ncdt` and `sh2o_gte_nccs` is the k kernel `sh2o_k_mvb3_w` (entry `sh2o_k_mvb3`, see the `k` section). The other operations are C only.

| Operation | SH2ZAM guide | SH2ZAM kernel | Model suite | Related, not equal |
|---|---|---|---|---|
| `sh2o_gte_read`, `sh2o_gte_write` | none | none | none | The register file. |
| `sh2o_gte_mvmva` | MAC.L row; plan prepared at the load | `saturn/src/sh2zam_batch_sh2.c:150` (`sh2z_transform_plan_execute_mac`) | `sh2-model` | Q16.16, no FLAG, no GTE limits. |
| `sh2o_gte_prepared_load`, `sh2o_gte_mvmva_prepared` (kernel `sh2oi_gte_prepared_mac3_l`, tag `gte-mvmva-prepared-1`) | MAC.L row; SR.S, read; plan prepared at the load (`saturn/BATCH_TRANSFORMS.md:29-58`, `:193-206`) | `saturn/src/sh2zam_prepared.c:48`, `saturn/src/sh2zam_batch_sh2.c:40`, `:17` | `sh2-model` (family `prepared`) | Not linked. The sh2o kernel uses the methods and gives the GTE bytes. SH2ZAM's kernel is Q16.16. Not on the default path. |
| `sh2o_gte_mvmva_list_prepared` (kernel `sh2oi_gte_prepared_mac3_l`, tag `gte-mvmva-list-prepared-1`) | `saturn/BATCH_TRANSFORMS.md:48-58`, `saturn/benchmarks/FUSED_SH2.md:1-7`; SR.S, read one time for each list | `saturn/src/sh2zam_batch_sh2.c:106` (`sh2zi_batch_mac_kernel`), `:132` | `sh2-model` (family `listprep`) | Not linked. The sh2o list loads its own shadow one time for each call and gives the GTE bytes for each item. SH2ZAM's kernel is Q16.16 over a point array. Not on the default path. |
| `sh2o_gte_nclip` | `saturn/VEHICLE_KERNELS.md:11`, `:24` | `saturn/src/sh2zam_vehicle.c:195` (`sh2z_shade_screen_facing`) | `vehicle-model` | SH2ZAM keeps or drops a face. NCLIP gives the signed value in MAC0. |
| `sh2o_gte_avsz3` | none | none | none | |
| `sh2o_gte_sqr` | none | none | none | |
| `sh2o_gte_gpf` | MAC.L row | `saturn/src/sh2zam_batch_sh2.c:40` | `sh2-model` | GTE shift, limits and FLAG. |
| `sh2o_gte_gpl` | MAC.L row | `saturn/src/sh2zam_batch_sh2.c:40` | `sh2-model` | The same. |
| `sh2o_gte_dpcs` | `saturn/SHADE.md:30` (blend with the fog colour), `:34-36` | `saturn/src/sh2zam_shade.c:262` (`sh2z_shade_fog_lookup`) | `shade-model` | SH2ZAM uses a threshold table and its own rounding. |
| `sh2o_gte_dpct` | `saturn/SHADE.md:30`, `saturn/SHADE_FUSED.md:3-6` | `saturn/src/sh2zam_shade_fused.c:684` (`sh2z_shade_fog_gouraud`) | `shade-model`, `shade-fused-model` | Related, not equal: SH2ZAM does the fog and the colour of all corners in one call, with its own table and rounding. It decides nothing for `sh2o_gte_dpct`, whose answers come from the GTE model. |
| `sh2o_gte_rtps` | `saturn/VIEW_BATCH.md:3-6`, `:29-33` | `saturn/src/sh2zam_view.c:151` (`sh2z_view_batch`) | `view-model` | Different formats, an exact divide in place of the GTE divide, no FLAG. |
| `sh2o_gte_rtpt` | `saturn/VIEW_BATCH.md:3-6`, `:29-33` | `saturn/src/sh2zam_view.c:151` | `view-model` | The same. |
| `sh2o_gte_nrtps` | `saturn/VIEW_BATCH.md:3-6`, `:29-33` | `saturn/src/sh2zam_view.c:151` | `view-model` | As for `sh2o_gte_rtps`. |
| `sh2o_gte_nrtpt` | `saturn/VIEW_BATCH.md:3-6`, `:29-33` | `saturn/src/sh2zam_view.c:151` | `view-model` | As for `sh2o_gte_rtpt`. |
| `sh2o_gte_ncs` | `saturn/SHADE.md:28-29` | `saturn/src/sh2zam_shade.c:61` (`sh2z_shade_light_normals`) | `shade-model` | int8 normals, Q14 light, its own limits. |
| `sh2o_gte_nct` | `saturn/SHADE.md:28-29` | `saturn/src/sh2zam_shade.c:61` | `shade-model` | The same. |
| `sh2o_gte_ncds` | `saturn/SHADE.md:28-30` | `saturn/src/sh2zam_shade.c:61` | `shade-model` | The same. |
| `sh2o_gte_ncdt` | `saturn/SHADE.md:28-30` | `saturn/src/sh2zam_shade.c:61` | `shade-model` | The same. |
| `sh2o_gte_nccs` | `saturn/SHADE.md:28-30` | `saturn/src/sh2zam_shade.c:61` | `shade-model` | The same. |

## In sh2o today: `psyq`

All these operations are GTE-exact: they give the bytes of the PSY-Q `libgte` functions (see [PSYQ.md](PSYQ.md)).

| Operation | SH2ZAM guide | SH2ZAM kernel | Model suite | Related, not equal |
|---|---|---|---|---|
| `sh2o_psyq_ApplyMatrix` | `saturn/MATH_OPTIMIZATIONS.md:46` | `saturn/src/sh2zam_transform.c:83` (`sh2z_mat33_transform`) | `math-model` | Q16.16 and saturation. `libgte` uses the GTE in Q12. |
| `sh2o_psyq_ApplyMatrixLV` | `saturn/MATH_OPTIMIZATIONS.md:46` | `saturn/src/sh2zam_transform.c:83` | `math-model` | The same. |
| `sh2o_psyq_CompMatrix` | `saturn/MATH_OPTIMIZATIONS.md:45` | `saturn/src/sh2zam_transform.c:135` (`sh2z_mat34_multiply`) | `math-model` | The same. |
| `sh2o_psyq_InitGeom` | none | none | none | Register setup. |
| `sh2o_psyq_PopMatrix` | none | none | none | |
| `sh2o_psyq_PushMatrix` | none | none | none | |
| `sh2o_psyq_RotTrans` | `saturn/MATH_OPTIMIZATIONS.md:46` | `saturn/src/sh2zam_transform.c:149` (`sh2z_mat34_transform_point`) | `math-model` | Q16.16 and saturation. |
| `sh2o_psyq_SetBackColor` | none | none | none | Register load. |
| `sh2o_psyq_SetColorMatrix` | none | none | none | Register load. |
| `sh2o_psyq_SetFarColor` | none | none | none | Register load. |
| `sh2o_psyq_SetFogNearFar` | `README.md:53`, `saturn/MATH_OPTIMIZATIONS.md:98` | `saturn/src/sh2zam.c:36` (`sh2z_muldiv`) | `math-model` | Equal only for a part of the inputs. See the delegate. |
| `sh2o_psyq_SetGeomOffset` | none | none | none | Register load. |
| `sh2o_psyq_SetGeomScreen` | none | none | none | Register load. |
| `sh2o_psyq_SetLightMatrix` | none | none | none | Register load. |
| `sh2o_psyq_SetRotMatrix` | Plan prepared at the load | `saturn/src/sh2zam_prepared.c:48` | `sh2-model` | A register load. It is the place where a prepared matrix is made. |
| `sh2o_psyq_SetTransMatrix` | Plan prepared at the load | `saturn/src/sh2zam_prepared.c:48` | `sh2-model` | The same. |
| `sh2o_psyq_TransMatrix` | none | none | none | |
| `sh2o_psyq_TransposeMatrix` | `README.md:56` | `saturn/src/sh2zam_transform.c:92` (`sh2z_mat33_transpose`) | none | Proven equal in the delegate suite. See the delegate. |
| `sh2o_psyq_ratan2` | `saturn/MATH_OPTIMIZATIONS.md:212-216` | `saturn/src/sh2zam_trig.c:128` (`sh2z_atan2`) | `math-model` | Not equal: the delegate suite finds differences. sh2o keeps its own table. |

## In sh2o today: the list and the delegate

The list is GTE-exact (see [GTE.md](GTE.md#the-mvmva-list)). The delegate is the CMake option `SH2O_ZAM`, off by default (see [ZAM.md](ZAM.md)). It gives work to SH2ZAM only for the inputs where a test proves equal bytes, and it refuses the other inputs.

| Operation | SH2ZAM guide | SH2ZAM kernel | Model suite | Related, not equal |
|---|---|---|---|---|
| `sh2o_gte_mvmva_list` | `saturn/BATCH_TRANSFORMS.md:48-58`, `saturn/benchmarks/FUSED_SH2.md:1-7` | `saturn/src/sh2zam_batch_sh2.c:106` (`sh2zi_batch_mac_kernel`) | `sh2-model` | Not linked. The sh2o list does one GTE MVMVA for each item, on the calling CPU. 88 vectors exact. |
| `sh2o_zam_mac3` | `saturn/BATCH_TRANSFORMS.md:185-206` | `saturn/src/sh2zam_batch_sh2.c:150`, `:132` (`sh2z_transform_plan_mac_available`) | `sh2-model` | Linked with the option. Equal for MVMVA (sf 1, mx < 3, cv other than 2), RTPS and RTPT inside the gate. It refuses the other inputs. Slower than the GTE core in the model. |
| `sh2o_zam_mvmva_list` | `saturn/BATCH_TRANSFORMS.md:29-58`, `:185-206` | `saturn/src/sh2zam_prepared.c:48`, `saturn/src/sh2zam_batch_sh2.c:150` | `sh2-model` | Linked with the option. Equal for the runs that it accepts. It refuses the others. No sh2o code calls it. |
| `sh2o_zam_SetFogNearFar` | `saturn/MATH_OPTIMIZATIONS.md:98` | `saturn/src/sh2zam.c:36` (`sh2z_muldiv`) | `math-model` | Linked with the option. Equal for the inputs that the delegate accepts. It refuses the others. No sh2o body calls it. |
| `sh2o_zam_TransposeMatrix` | `README.md:56` | `saturn/src/sh2zam_transform.c:92` | none | Linked with the option. Equal in all cases of the delegate suite. No sh2o body calls it. |
| `sh2o_zam_ratan2` | `saturn/MATH_OPTIMIZATIONS.md:212-216` | `saturn/src/sh2zam_trig.c:128` | `math-model` | Refused. `sh2z_atan2` does not give the `libgte` bytes. sh2o keeps its own table. |

## To come: new operations and kernels

None at this time.
