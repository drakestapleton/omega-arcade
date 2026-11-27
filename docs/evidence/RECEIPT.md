# `omega-arcade` Verification, Parity & Benchmark Receipt

- **Evidence Labels:** `MEASURED_DGX_SPARK_GRACE_A64` (pinned Cortex-X925 CPU `15`), `MEASURED_APPLE_SILICON_MACOS` (native `a64macos` Mach-O)
- **Verdict:** **PASS** — `11/11` security & negative-control checks (`0 FAIL`), **5-way bit-exact SHA-256 parity** across `Pure Omega (a64linux) == Pure Omega (a64macos) == C11 (gcc -O3) == Node.js (v24.21.0) == Python 3 (3.12.3)` over `3 × 300` frames (`207,360,000` rendered pixels / `622,080,000` RGB bytes), all 3 static ELFs `< 50 KiB`, peak `VmHWM` `< 1.0 MiB` (`968–976 KB`).
- **Timestamp (UTC):** `2026-11-27T02:08:44Z`
- **Spark Host:** `Linux spark-b87b 7.0.0-1019-nvidia #19~24.04.2-Ubuntu SMP PREEMPT_DYNAMIC Sun Sep 6 18:35:11 UTC 2 aarch64`
- **CPU Pin & Hardware Hygiene:** `taskset -c 15` (dedicated Cortex-X925 core `15`, `CPU part 0xd87`). Sibling cores `5, 7, 8, 9` untouched; `/dev/nvidia*` and `/dev/aien-gpu` never opened.
- **Compiler & Runtime Pins:**
  - Spark seeded `omc` (`a64linux`): `~/canon/omega-mv/seed/out/omc` — SHA-256 `28fca13d44054e26f78c33d1753fd5c005d08e9144d95985c90bd75b60885072`
  - MacBook native `omc` (`a64macos`): `~/.local/bin/omc` — SHA-256 `68a590c37dfb1ec37a183f0032e02db1570db299266f574f65556edfc70feb9c` (ad-hoc signed `codesign -s - -f`)
  - C11 compiler (Spark): `gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0` (`-O3 -mcpu=native -Wall -Wextra`, 0 warnings)
  - Node.js (Spark): `v24.21.0`
  - Python 3 (Spark): `Python 3.12.3`
- **Machine-Readable Data & Raw Log:** [`results.json`](./results.json), [`spark_run.log`](./spark_run.log)
- **Checksums:** [`SHA256SUMS.txt`](./SHA256SUMS.txt) (`shasum -a 256 -c docs/evidence/SHA256SUMS.txt`)

---

## 1. Binary Size & Static Linkage Audit (Spark Linux AArch64)

Every Pure-Omega game engine compiles from pure `.om` to a standalone zero-libc static AArch64 ELF under `50 KiB` (`40.77–47.05 KiB`), with zero heap allocator (`0` dynamic allocations), zero SDL/OpenGL/X11, and zero shared libraries:

| Binary | Kind | Size (Bytes) | Size (KiB) | Linkage (`file` / `ldd`) | SHA-256 (Spark build) |
|---|---|---:|---:|---|---|
| `bin/raycast_doom` | Pure OM-2 3D DDA Raycaster | `41,752` | `40.77` | Static AArch64 ELF (`not a dynamic executable`, zero libc) | `6aafc5c3ea524054da5892711d81b0cc0ae554fb9540cee58c4aeefd10f5486c` |
| `bin/blocks_puzzle` | Pure OM-2 7-Bag SRS Falling Blocks | `43,832` | `42.80` | Static AArch64 ELF (`not a dynamic executable`, zero libc) | `cb5f3b68d618e315e3c17667f7ff1439824cb060a107252c105956ea1e90eb27` |
| `bin/starfighter` | Pure OM-2 4-Strand Vector Starfighter | `48,184` | `47.05` | Static AArch64 ELF (`not a dynamic executable`, zero libc) | `3c4cb4b125017929abc41b9b61f9c26aa5d19cca558ad33e3298ebbb6123d4b1` |
| `bin/gate` | Pure OM-2 10-Check Security & Tamper Gate | `33,448` | `32.66` | Static AArch64 ELF (`not a dynamic executable`, zero libc) | `9228cf4487c9e1e2037507b2e25db027771b4d60823835a1f3f724d64649dec3` |
| `bin/probe_oob` | OOB Sprite Trap 3 Probe (`exit 103`) | `324` | `0.32` | Static AArch64 ELF | `83d1a8618b60e5df3f97bb3a87670edb3aebec71ef7f0769773b3b83edfcad99` |
| `bin/probe_overflow` | Signed Overflow Trap 1 Probe (`exit 101`) | `304` | `0.30` | Static AArch64 ELF | `3933496871691a7362e7f5cae905268b741768257a14e469087a9b2280ef2205` |
| `bin/probe_div0` | Div-by-Zero Trap 2 Probe (`exit 102`) | `312` | `0.30` | Static AArch64 ELF | `0f32aadb207611b3ad770aed5d6246e17128c18bd03ee430bcae7107ea675f71` |
| `bin/probe_loop` | Loop-Bound Trap 4 Probe (`exit 104`) | `296` | `0.29` | Static AArch64 ELF | `21a0d9e571c4b45eef1c607bb4e77820b24aff03f6f5c9dde3a4bf3dbb337fce` |
| `bin/probe_escape` | `execpure` Hostile Mod Escape Probe | `1,364` | `1.33` | Static AArch64 ELF | `eae32ee8d748fd9b7864a0ae03f4be65b9418d6dcf84a36ca70e30e65dae0398` |
| `bin/c/raycast_doom` | C11 `gcc -O3 -mcpu=native` reference | `71,648` | `69.97` | Dynamic PIE (`libc.so.6`, `ld-linux-aarch64.so.1`) | — |
| `bin/c/blocks_puzzle` | C11 `gcc -O3 -mcpu=native` reference | `72,040` | `70.35` | Dynamic PIE (`libc.so.6`, `ld-linux-aarch64.so.1`) | — |
| `bin/c/starfighter` | C11 `gcc -O3 -mcpu=native` reference | `72,648` | `70.95` | Dynamic PIE (`libc.so.6`, `ld-linux-aarch64.so.1`) | — |

---

## 2. 5-Way Bit-Exact Frame SHA-256 Parity (`EXACT_5WAY_PARITY`)

All 5 independent build/execution targets — **Pure Omega (`a64linux` Spark CPU 15)**, **Pure Omega (`a64macos` Apple Silicon)**, **C11 (`gcc -O3`)**, **Node.js (`v24.21.0`)**, and **Python 3 (`3.12.3`)** — executed the exact same 300-frame deterministic replay (`640×360` = `69,120,000` pixels per game; `207,360,000` pixels total) and produced **100% identical cryptographic SHA-256 hashes** on:
1. `HERO_PPM_SHA256`: FIPS 180-4 SHA-256 over the full `691,215`-byte `P6` PPM hero frame (also cross-checked byte-for-byte against raw `ppm` stdout).
2. `FINAL_PPM_SHA256`: FIPS 180-4 SHA-256 over the full `691,215`-byte `P6` PPM final frame (`frame 299`).
3. `STREAM_SHA256`: FIPS 180-4 SHA-256 over all `300` big-endian 32-bit FNV-1a full-frame digests (`1,200` bytes covering every single pixel of frames `0..299`).

| Game | Hero Frame | `HERO_PPM_SHA256` (691,215 B P6 PPM) | `FINAL_PPM_SHA256` (Frame 299) | `STREAM_SHA256` (All 300 Frames) | 5-Way Verdict |
|---|---:|---|---|---|---|
| `raycast_doom` | `185` | `6bac57339b917c42d89f9ed8481ae08f7b422d0c9f26fe3f48b97351ea736a52` | `daff8fe7b40653109eb56986364488b20160f7eb6083b8276d72921f935d3509` | `fcddd5379dba1f73958f9d32f70ae46146fc58050237890b5255404b87379923` | **PASS (5/5 EXACT)** |
| `blocks_puzzle` | `248` | `ddea108ea8b9649dcab8055006f5041a4003c4f567c304c3e77a015f2e8b7535` | `28256ac4e1ecced533f07df2a159b6c0862cac85713a32d8c960a1471d70ca4d` | `95ea8731949096c035733e0288f306830551ada4e7cc26a20f1e449d2ef1185b` | **PASS (5/5 EXACT)** |
| `starfighter` | `195` | `cf1ca2191b3c3f31249f8dd6a4d51e5d1ec9180c15dfeb9a5bf3125e6eb72edf` | `77dc9aa56eaf726c5509c4d19be784204e518b53368c61818fed1396462af8d2` | `f7ee03c4093b61cb32ac513aa27b16666bd1f04fa368b66107bf2291a9aac82b` | **PASS (5/5 EXACT)** |

Showcase JPEG artifacts rendered directly from the verified Pure-Omega PPM outputs (`PIL`, quality 93, `4:4:4` subsampling):
- `docs/screenshots/raycaster.jpg` — `56,928 B` (`55.6 KiB`), SHA-256 `fc5144e22217750e86003aa8147021362d42e30fa6eb9f8cdd34bd01e475599a`
- `docs/screenshots/blocks.jpg` — `79,240 B` (`77.4 KiB`), SHA-256 `dcae1da0a2b66f127f53e92dbf090a03be875df174b02a61c9c233c40b98b424`
- `docs/screenshots/starfighter.jpg` — `55,246 B` (`54.0 KiB`), SHA-256 `0c3a332835d5480824986dd28600c087e73320a532a85e8b92b8127e696943ee`

---

## 3. Paired Interleaved Head-to-Head Benchmark (`MEASURED_DGX_SPARK_GRACE_A64`, CPU `15`)

Command: `BENCH_CPU=15 TRIALS=3 FRAMES=300 bash bench/bench_arcade.sh`  
Peak RAM (`VmHWM`) is read directly from Linux `/proc/self/status` inside each process right before exit (eliminating parent `fork()` `hiwater_rss` contamination). Every run verifies matching `ANTI_DCE` (360-pixel-per-frame scanline hash) and `FINAL_PPM_SHA256`.

### 3.1 Summary Medians (3 Paired Interleaved Trials × 300 Full `640×360` Frames)

| Game | Engine | Median FPS | Median µs/frame | 300-Frame Time | Peak RAM (`VmHWM`) | Speed vs Pure OM-2 | RAM vs Pure OM-2 |
|---|---|---:|---:|---:|---:|---:|---:|
| **`raycast_doom`** | **Pure OM-2 (`a64linux`)** | **`1,609.95`** | **`621.14 µs`** | **`186.342 ms`** | **`972 KB`** (`0.95 MiB`) | **`1.00×`** | **`1.00×`** |
| `raycast_doom` | C11 (`gcc -O3 -mcpu=native`) | `6,151.83` | `162.55 µs` | `48.766 ms` | `2,140 KB` | `3.82×` faster | `2.20×` more RAM |
| `raycast_doom` | Node.js `v24.21.0` (V8 JIT) | `1,893.01` | `528.26 µs` | `158.478 ms` | `59,836 KB` | `1.18×` faster | **`61.56×` more RAM** |
| `raycast_doom` | Python `3.12.3` | `34.56` | `28,937.95 µs` | `8,681.384 ms` | `22,388 KB` | **`46.58×` slower** | **`23.03×` more RAM** |
| **`blocks_puzzle`** | **Pure OM-2 (`a64linux`)** | **`2,808.09`** | **`356.11 µs`** | **`106.834 ms`** | **`968 KB`** (`0.95 MiB`) | **`1.00×`** | **`1.00×`** |
| `blocks_puzzle` | C11 (`gcc -O3 -mcpu=native`) | `15,946.80` | `62.71 µs` | `18.813 ms` | `2,204 KB` | `5.68×` faster | `2.28×` more RAM |
| `blocks_puzzle` | Node.js `v24.21.0` (V8 JIT) | `1,614.24` | `619.49 µs` | `185.846 ms` | `57,780 KB` | **`1.74×` slower** | **`59.69×` more RAM** |
| `blocks_puzzle` | Python `3.12.3` | `38.48` | `25,990.55 µs` | `7,797.166 ms` | `22,012 KB` | **`72.98×` slower** | **`22.74×` more RAM** |
| **`starfighter`** | **Pure OM-2 (`a64linux`)** | **`5,649.69`** | **`177.00 µs`** | **`53.100 ms`** | **`976 KB`** (`0.95 MiB`) | **`1.00×`** | **`1.00×`** |
| `starfighter` | C11 (`gcc -O3 -mcpu=native`) | `34,404.02` | `29.07 µs` | `8.720 ms` | `2,148 KB` | `6.09×` faster | `2.20×` more RAM |
| `starfighter` | Node.js `v24.21.0` (V8 JIT) | `5,431.12` | `184.12 µs` | `55.237 ms` | `60,740 KB` | **`1.04×` slower** | **`62.23×` more RAM** |
| `starfighter` | Python `3.12.3` | `166.24` | `6,015.41 µs` | `1,804.622 ms` | `18,884 KB` | **`33.99×` slower** | **`19.35×` more RAM** |

### 3.2 Raw Per-Trial Measurements (Spark CPU `15`)

| Game | Trial | OM-2 FPS / µs / `VmHWM` | C11 FPS / µs / `VmHWM` | Node v24 FPS / µs / `VmHWM` | Python 3 FPS / µs / `VmHWM` |
|---|---:|---|---|---|---|
| `raycast_doom` | 1 | `1,607.11` / `622.24` / `972 KB` | `6,151.83` / `162.55` / `2,140 KB` | `1,867.44` / `535.49` / `59,708 KB` | `34.65` / `28,858.83` / `22,384 KB` |
| `raycast_doom` | 2 | `1,610.12` / `621.07` / `968 KB` | `6,154.10` / `162.49` / `2,136 KB` | `1,984.82` / `503.82` / `59,836 KB` | `34.56` / `28,937.95` / `22,456 KB` |
| `raycast_doom` | 3 | `1,609.95` / `621.14` / `972 KB` | `6,103.55` / `163.84` / `2,140 KB` | `1,893.01` / `528.26` / `60,036 KB` | `34.41` / `29,061.97` / `22,388 KB` |
| `blocks_puzzle` | 1 | `2,807.89` / `356.14` / `968 KB` | `15,946.80` / `62.71` / `2,208 KB` | `1,626.36` / `614.87` / `57,780 KB` | `38.76` / `25,800.14` / `22,012 KB` |
| `blocks_puzzle` | 2 | `2,808.09` / `356.11` / `972 KB` | `15,938.86` / `62.74` / `2,204 KB` | `1,614.24` / `619.49` / `57,844 KB` | `38.48` / `25,990.55` / `22,012 KB` |
| `blocks_puzzle` | 3 | `2,808.26` / `356.09` / `968 KB` | `16,059.97` / `62.27` / `2,204 KB` | `1,609.26` / `621.40` / `57,224 KB` | `37.79` / `26,459.23` / `22,008 KB` |
| `starfighter` | 1 | `5,650.01` / `176.99` / `976 KB` | `34,682.63` / `28.83` / `2,148 KB` | `5,431.12` / `184.12` / `60,776 KB` | `166.24` / `6,015.41` / `18,884 KB` |
| `starfighter` | 2 | `5,649.69` / `177.00` / `980 KB` | `34,404.02` / `29.07` / `2,144 KB` | `5,317.00` / `188.08` / `60,740 KB` | `166.14` / `6,018.98` / `18,884 KB` |
| `starfighter` | 3 | `5,634.88` / `177.47` / `976 KB` | `34,320.01` / `29.14` / `2,152 KB` | `5,512.13` / `181.42` / `59,812 KB` | `166.28` / `6,014.08` / `18,884 KB` |

### 3.3 Apple Silicon Native Mach-O Verification (`MEASURED_APPLE_SILICON_MACOS`)

Compiled from the exact same `.om` game sources with `om/hdr_macos.om` (`target a64macos`) and ad-hoc signed (`codesign -s - -f`):

| Game | 3-Trial Median FPS | Median µs/frame | 300-Frame Time | 4 Hardware Trap Probes | Parity vs Spark `a64linux` |
|---|---:|---:|---:|---|---|
| `raycast_doom` | **`1,143.15 FPS`** | `874 µs` | `262.43 ms` | `oob=103`, `ovf=101`, `div0=102`, `loop=104` | **EXACT (`HERO`, `FINAL`, `STREAM`)** |
| `blocks_puzzle` | **`1,798.23 FPS`** | `556 µs` | `166.83 ms` | `oob=103`, `ovf=101`, `div0=102`, `loop=104` | **EXACT (`HERO`, `FINAL`, `STREAM`)** |
| `starfighter` | **`3,217.39 FPS`** | `310 µs` | `93.24 ms` | `oob=103`, `ovf=101`, `div0=102`, `loop=104` | **EXACT (`HERO`, `FINAL`, `STREAM`)** |

---

## 4. Honest Caveats & Scope Boundaries

1. **C11 `gcc -O3 -mcpu=native` beats Pure OM-2 by `3.82×–6.09×` on raw CPU throughput:** GCC auto-vectorizes framebuffer row fills with ARMv9 NEON/SVE stores, omits all array bounds checks (`CWE-787`), treats signed overflow as undefined behavior (`CWE-190`), omits loop-bound trip counters (`CWE-835`), and aggressively inlines helper functions across translation boundaries. Pure OM-2 enforces mandatory runtime bounds traps (`trap 3`), signed overflow traps (`trap 1`), division-by-zero traps (`trap 2`), and loop bounds (`trap 4`) inside a non-vectorizing single-pass seeded compiler (`omc`), while still sustaining **`1,610–5,650 FPS`** (`26×–94×` above 60 FPS) inside **`< 1 MiB` `VmHWM`**.
2. **Node.js `raycast_doom` vs Pure OM-2:** V8 TurboFan JIT is `1.18×` faster than Pure OM-2 on `raycast_doom` (`1,893` vs `1,610 FPS`), while Pure OM-2 beats Node.js on both `blocks_puzzle` (`1.74×` faster) and `starfighter` (`1.04×` faster) using **`59.7×–62.2×` less RAM** (`~972 KB` vs `~58–61 MB`) and **`~2,500×` smaller binary footprint** (`~44 KiB` vs `node` runtime).
3. **Headless software framebuffer scope:** These binaries render deterministic `640×360` RGB24 frames into fixed static slabs (`900 KiB` packed framebuffer) and emit standard `P6` PPM streams / cryptographic SHA-256 proofs. They do not open an interactive X11/Wayland/AppKit window, audio device, or hardware GPU context (`NO_GPU_TOUCHED`).
4. **Apple Silicon Mach-O binary size vs Linux ELF:** On macOS ARM64 (`a64macos`), Mach-O 16 KiB segment page alignment and ad-hoc code-signing padding yield `83,984–100,400 B` binaries for the exact same `.om` code that compiles to `41,752–48,184 B` (`40.77–47.05 KiB`) static Linux AArch64 ELFs.
