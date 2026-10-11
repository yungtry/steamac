# Compatibility

Games tested with the patched FEX in the SteamOS guest (see [README.md](README.md)).

| game | result | notes |
|---|---|---|
| Goose Goose Duck | works | everything works |
| Halo: The Master Chief Collection | works | on loading into a map it may hitch at first while shaders are cached |
| The Division 2 | fails | gets past EAC, then hangs at VMProtect / Ubisoft Connect |
| Halo Infinite | doesn't work | compiling shaders takes a long time; blockers in [Halo Infinite](#halo-infinite) |

## Halo Infinite

**Not playable** under steamac (2026-10-11, KosmicKrisp with 0048, Proton Experimental 11.0, vkd3d-proton 3.1). The
blockers, in the order the game reaches them:

| stage | symptom | cause and status |
|---|---|---|
| start | the whole VM dies | KosmicKrisp crash on an R32G32B32 texel buffer view (see [eac-troubleshooting.md](../docs/eac-troubleshooting.md)). **Fixed by 0048**: no VM crash in two launches after the swap |
| startup video | spinning circle, then a crash to desktop; `DumpTool.exe` starts. Proton log: DXVK `Failed to open shared D3DKMT handle`, then vkd3d-proton `D3D12-Fence shared timeline semaphores not supported by host` and `d3d12_device_OpenSharedHandle: … hr 0x80004001`, then the game's own exception `0xe343000a` | the game decodes the startup video on a D3D11 device and hands the frames to D3D12 through a shared texture and a shared `ID3D12Fence`. Venus exposes no exportable timeline semaphores (`external_timeline_semaphore_handles = 0` in `vn_physical_device.c`; only binary SYNC_FD), so the import fails and the game aborts. **Workaround**: move `videos/intro.mp4` and `videos/Startup_Sequence_Loading.mp4` out of the game directory (Steam's *Verify integrity* restores them); the same path is expected to fail for the other in-game videos. Fixing it needs shareable timeline semaphores across Venus, virglrenderer and KosmicKrisp |
| first start | spinning circle for a long time, no network traffic to Xbox Live | startup pipeline precompile of ~13.7k PSOs (the count is the first `uint32` of `deploy/pc/psoDictionary.bin`) on one game thread, at 3–5 PSOs/s: the DXIL→SPIR-V translation runs under FEX (vkd3d-proton is x86-64) and every pipeline is a synchronous Metal compile. Measured 48% after ~25 min, `vkd3d-proton.cache.write` at 160 MB. The vkd3d-proton cache in the game directory **does** persist (a second start loads it), so later starts skip the translation; the Metal compile repeats |
| after the precompile starts | black picture (the launcher's no-picture notice shows 100% black), game alive, alt-tab works | vkd3d-proton logs `d3d12_command_signature_create: Device generated commands is not supported by implementation` for the game's `ExecuteIndirect` command signatures. Halo Infinite's renderer needs `VK_EXT_device_generated_commands` (or the NV extension; RADV implemented it for this game); neither Venus nor KosmicKrisp implements it. **Open — the real blocker.** Fixing it needs DGC in KosmicKrisp (e.g. on Metal indirect command buffers) and its pass-through in Venus and virglrenderer |

For a diagnostic run, `PROTON_LOG=1` writes `~/steam-1240440.log` in the guest, but drop it for any timing: the game
`longjmp`s ~180 times/s on its worker threads once it leaves the spinner, and the `+seh,+unwind` channels log dozens of
lines per jump (60 MB in a few minutes).
