# Upstream watch list

Upstream changes that would retire patches this fork carries. Re-check every row when a component's pin is bumped; when
an item lands, drop or shrink the patch it names and update the row. Last checked: 2026-10-11.

## KosmicKrisp (Mesa, `host/kosmickrisp`)

Pin: Mesa `ce576c29`. MR state from `gitlab.freedesktop.org/api/v4/projects/mesa%2Fmesa/merge_requests/<id>`.

| upstream item | state | our patches it would retire |
|---|---|---|
| [!44786](https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/44786) kk: Add geometry shader support | open (updated 10-05) | 0002–0009, 0012 (GS emulation, adjacency, GS metadata, stream selection) |
| [!44928](https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/44928) Draft: kk: Implement VK_EXT_transform_feedback | draft (updated 10-09) | 0010, 0011, 0013–0015 |
| [!44990](https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/44990) kk: Allow LINEAR color attachments | open (updated 10-07) | 0018 (compare 0019, 0021 when it lands) |
| [!44221](https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/44221) kk: Add a device-local-only memory type | open (updated 09-04) | 0017 |
| [!44222](https://gitlab.freedesktop.org/mesa/mesa/-/merge_requests/44222) kk: Create linear array textures as 2D plus a view | open (updated 09-04) | 0020 |

## virglrenderer (`host/virglrenderer`)

Pin: `utmapp/virglrenderer` `5d26f605`. MR state from `.../projects/virgl%2Fvirglrenderer/merge_requests/<id>`.

| upstream item | state | our patches it would retire |
|---|---|---|
| [!1685](https://gitlab.freedesktop.org/virgl/virglrenderer/-/merge_requests/1685) vkr: import shared memory as host memory instead of an MTLBuffer | open (updated 10-07); its MoltenVK dependency (KhronosGroup/MoltenVK#2834) is merged | 0002, 0011 |
| [!1688](https://gitlab.freedesktop.org/virgl/virglrenderer/-/merge_requests/1688) proxy: signal fences through a pipe where there's no eventfd | open (updated 10-05) | 0004, 0009 |
| [!1692](https://gitlab.freedesktop.org/virgl/virglrenderer/-/merge_requests/1692) vkr: device local mem type spoof | open (updated 10-08) | 0011 (with KosmicKrisp 0017) |

## libkrun (`host/libkrun`)

Pin: `v1.19.6`, the latest 1.x tag. `main` is 2.0 development; the `stable-1.19.x` branch has no commits past `v1.19.6`.
The fixes below are on `main` only, so a 1.19.x tag retires them only if they are backported.

| upstream commit (main) | our patch it would retire |
|---|---|
| `51367cfaf379` virtio/gpu: offer VIRTIO_GPU_F_BLOB_ALIGNMENT | 0001 |
| `e328555a447d` virtio/input: keep select and subsel when a config query fails | 0004 |
| `6a8c35347833` virtio/input: report the full size of VIRTIO_INPUT_CFG_ABS_INFO | 0005 |
| `8b202e6e0e22` vmm,hvf,libkrun: add krun_vm_pause/resume (2.0 reworks it as `VmmHandle`, `a92e9b216df7`) | 0016, if the 1.x API matches ours |

The audio patches (0014, 0017, 0018, 0020, 0022, 0023) have no upstream counterpart.

## MoltenVK (`host/moltenvk`, fallback driver)

Pin: `utmapp/MoltenVK` `geometry-shaders` `05604465` (KhronosGroup `main` `f79c6c5` + 20 commits), SPIRV-Cross
`utmapp` `939b40b`. Khronos `main` (`67b26826`) already contains:

| upstream commit | our patch it retires |
|---|---|
| MoltenVK `74850136` emulate reversed-depth viewports on affected AMD Mac GPUs; SPIRV-Cross #2640 | moltenvk 0001, spirv-cross 0001 |
| MoltenVK `a8142505` VK_EXT_depth_clip_enable; SPIRV-Cross #2666 | moltenvk 0002, spirv-cross 0002 |
| MoltenVK `2da7c3cb` prevent null dereference in MVKPixelFormats | moltenvk 0009 |
| MoltenVK `c0af3a56` retain query pools until Metal command buffer completion | moltenvk 0022 (compare) |
| MoltenVK `226b6edb` crash binding a null descriptor set | moltenvk 0034 (compare 0023) |
| MoltenVK `701747d6` fix areQueriesDeviceAvailable (#2794) | moltenvk 0027 |
| MoltenVK `2c8ec932` host pointer imports on non-host-visible memory types | the same change in the utmapp base |
| SPIRV-Cross `cd3fcb26` reserve Metal type names | spirv-cross 0008 shrinks to the `array` keyword |

Still not upstream: geometry shaders and transform feedback (KhronosGroup/MoltenVK draft PRs #1815, #2786, #2812, #1943).
The bump needs the utmapp geometry-shader branch merged onto Khronos `main` (conflicts in the pipeline, draw and
implicit-buffer code, which upstream reworked for `gl_DrawID` and depth-clip state) and a build with Xcode.
