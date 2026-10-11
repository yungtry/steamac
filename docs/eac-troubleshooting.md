# EAC troubleshooting

## Verify success

* EAC launcher log (`anticheatlauncher.log`, in the `EAC_LAUNCHERDIR` directory): `System name 'linux64'`, `Starting Wine
  module mapping`, then `Launcher finished with: 301, 'Easy Anti-Cheat successfully loaded in-game'` (about 5 s).
* VRChat `output_log_*.txt`: `[AntiCheatClient] … bound`, `AntiCheat Session Begin: Success`, `Finished entering world`,
  and **no** `null client` (that means a stub loaded, not EAC).

Never attach a debugger, `strace` or `perf` to the running game: EAC reports it. Use the logs only.

## Symptoms

| symptom | check |
|---|---|
| launcher stops at `Starting Wine module mapping`, or the result is not `301` | the patched FEX is probably not installed: `sh install-fex-tool.sh status`. Valve's FEX-2607 fails EAC ([eac-how-it-works.md](eac-how-it-works.md)) |
| `Starting Wine module mapping`, then `Launcher finished with: 210, 'Unexpected error. (#1)'` about 3 s later, with the patched FEX installed | the FEX build predates patch 0027: the current EAC launcher module injects into the guest before its libc is loaded and picked FEX's own aarch64 `libc.so.6` from `/proc/<pid>/maps`. Re-run `sh build-fex.sh` (it applies only the missing patches) and `sh install-fex-tool.sh` |
| `Starting Wine module mapping`, then `Launcher finished with: 210, 'Unexpected error. (#2) (0)'` under a second later; the Proton log may also show `EACLaunch.exe` crashing (`page fault on read access to 0xfffffffffffffffa`) | the FEX build predates patch 0030: the launcher's ptrace view showed it FEX's own `SIGBUS` stops for unaligned atomics in the game process, and it gave up after 1000. Re-run `sh build-fex.sh` and `sh install-fex-tool.sh` |
| Ubisoft Connect (The Division 2) stays at "Looking for patches"; its `launcher_log.txt` shows `Certificate verification ... failed`, or the Proton log shows `schan_DecryptMessage Returning c0000005` / a `handle_syscall_fault` in `libnettle.so.8` or `libhogweed` | the FEX build predates patch 0029: re-run `sh build-fex.sh` and `sh install-fex-tool.sh` |
| worked before, fails after a Steam update | the update replaced the FEX tool with Valve's build; re-run `sh install-fex-tool.sh` |
| the Android build starts (or the game does not use Proton) | set the game's compatibility tool to Proton Experimental and let Steam install the Windows depot |
| EAC runtime errors at start | install **Proton EasyAntiCheat Runtime** (1826330) and check `EAC_LAUNCHERDIR` / `PROTON_EAC_RUNTIME` in the launch options |
| ~60 s stall before joining (region lookup) | keep `WINE_CPU_TOPOLOGY=16:…` in the launch options; with it the region lookup took 4–6 s |
| game freezes after a microphone start, or after an in-game settings/resolution change that reinitialises the audio device; guest log shows `virtio_snd: control message (0x00000103/0x00000104/0x00000105) timeout` and the game's main thread spins | the CoreAudio capture start blocked while holding the per-stream `capture` mutex, which the snd pump thread also takes every iteration (while it holds `hosts`, which the control thread needs to answer PREPARE/START/STOP) — so *every* virtio-snd control message timed out. Root-cause fix: libkrun **0019**, which starts the capture unit outside that lock ([fork.md](fork.md)) |
| log flooded (~250 lines/s) with capture errors | macOS microphone permission is missing for the app; grant it |
| stutter in new areas | expected: Venus has no `VK_EXT_graphics_pipeline_library` and KosmicKrisp compiles each new pipeline synchronously (50–100 ms typical; a single Metal compile has been observed over 60 s, and a 349 s GPU-idle stall at world load). **The pipeline caches do not persist** — the DXVK state cache and the FOSSILIZE bucket stay empty and the Venus/Mesa cache holds headers only — so the work repeats each run. Asynchronous compilation (`MESA_KK_ASYNC_PIPELINES=1`, KosmicKrisp 0041) previously crashed the VM under VRChat; the real gap was the draw gate watching the bound VS rather than the program the flush reads, fixed by KosmicKrisp 0047 and **verified live** under `MESA_KK_ASYNC_PIPELINES=2` ([fork.md](fork.md)) |
| the VM itself dies (macOS report `steamac-vm-*.ips` in `~/Library/Logs/DiagnosticReports`, `EXC_BAD_ACCESS … at 0x000000000000000a`, faulting thread `vkr-ring-*` in `kk_CreateBufferView`) shortly after a D3D12 game's start — seen with Halo Infinite after its unsupported-GPU notice | the game made a typed buffer SRV of an R32G32B32 format, which Metal has no pixel format for, and KosmicKrisp dereferenced the missing format entry. Fixed by KosmicKrisp **0048** (such views are read as R32 texture buffers): rebuild with `host/kosmickrisp/build.sh` and put the new driver in the app with `host/kosmickrisp/swap-app-dylib.sh swap` (`restore` puts the release driver back; [fork.md](fork.md)). With MoltenVK the same request aborts in `MVKDevice::getVkFormatTexelBufferAlignment` — use KosmicKrisp for such games. Halo Infinite then hits the blockers in [compatibility.md](../fex-eac/compatibility.md#halo-infinite) |
| rebuilt app asks for microphone / keychain again | expected after re-signing ([fork.md](fork.md)) |
