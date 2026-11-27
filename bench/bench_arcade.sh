#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# bench/bench_arcade.sh: Reproducible Spark AArch64 build, 10-check security gate,
# bit-exact 300-frame parity check, and paired interleaved H2H FPS + VmHWM benchmark.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BENCH_CPU="${BENCH_CPU:-15}"
TRIALS="${TRIALS:-3}"
FRAMES="${FRAMES:-300}"
OMC="${OMC:-$HOME/canon/omega-mv/seed/out/omc}"
PINNED_OMC_SHA="28fca13d44054e26f78c33d1753fd5c005d08e9144d95985c90bd75b60885072"

cd "$ROOT"

echo "=== [1/5] Environment & Compiler Pin ==="
uname -a
echo "CPU_PIN=taskset -c $BENCH_CPU"
GOT_SHA="$(sha256sum "$OMC" | awk '{print $1}')"
echo "OMC_SHA256=$GOT_SHA"
if [ "$GOT_SHA" != "$PINNED_OMC_SHA" ]; then
  echo "FAIL: pinned omc mismatch (expected $PINNED_OMC_SHA)" >&2
  exit 2
fi
gcc --version | head -n 1
node --version
python3 --version

echo "=== [2/5] Build Pure-Omega Static ELFs (< 50 KiB) & C11 (-O3 -mcpu=native) ==="
bash build.sh
mkdir -p bin/c
for g in raycast_doom blocks_puzzle starfighter; do
  gcc -O3 -mcpu=native -Wall -Wextra "ref/c/${g}.c" -o "bin/c/${g}"
  om_sz="$(wc -c < "bin/${g}" | tr -d ' ')"
  c_sz="$(wc -c < "bin/c/${g}" | tr -d ' ')"
  echo "SIZE ${g}: om=${om_sz}B c=${c_sz}B"
  if [ "$om_sz" -ge 51200 ]; then
    echo "FAIL: bin/${g} (${om_sz}B) exceeds 50 KiB budget" >&2
    exit 1
  fi
done
echo "--- Linkage Audit ---"
for b in bin/raycast_doom bin/blocks_puzzle bin/starfighter bin/gate bin/c/raycast_doom; do
  file "$b"
  ldd "$b" 2>&1 || true
done

echo "=== [3/5] Pure-Omega Negative-Control & Security Gate (CPU $BENCH_CPU) ==="
taskset -c "$BENCH_CPU" ./bin/gate ./bin

echo "--- Compile-Time Static OOB Refusal Control (omc error 21) ---"
set +e
printf 'om2\nfn main to int\na = new uchar 4 0\nput a 8 uchar 1\nret 0\nend\n' | "$OMC" > /tmp/oob_compile_test.bin 2> /tmp/oob_compile_test.err
oob_rc=$?
set -e
oob_sz="$(wc -c < /tmp/oob_compile_test.bin | tr -d ' ')"
echo "STATIC_OOB_COMPILE_RC=$oob_rc BIN_BYTES=$oob_sz ERR=$(cat /tmp/oob_compile_test.err)"
rm -f /tmp/oob_compile_test.bin /tmp/oob_compile_test.err
if [ "$oob_rc" -ne 21 ] || [ "$oob_sz" -ne 0 ]; then
  echo "FAIL: expected compile-time error 21 with 0 emitted bytes" >&2
  exit 1
fi
echo "PASS G11_COMPILE_TIME_STATIC_OOB_ERR21"

echo "=== [4/5] 4-Way Spark Bit-Exact 300-Frame Parity (OM-2 vs C vs Node vs Python 3) ==="
python3 - "$ROOT" "$BENCH_CPU" "$FRAMES" << 'PYEOF'
import json, os, re, subprocess, sys

root, cpu, frames = sys.argv[1], sys.argv[2], int(sys.argv[3])
games = ["raycast_doom", "blocks_puzzle", "starfighter"]
engines = [
    ("om_a64linux", lambda g: ["taskset", "-c", cpu, f"{root}/bin/{g}", "verify", str(frames)]),
    ("c_gcc_o3",    lambda g: ["taskset", "-c", cpu, f"{root}/bin/c/{g}", "verify", str(frames)]),
    ("node_v24",    lambda g: ["taskset", "-c", cpu, "node", f"{root}/ref/node/{g}.js", "verify", str(frames)]),
    ("python3",     lambda g: ["taskset", "-c", cpu, "python3", f"{root}/ref/py/{g}.py", "verify", str(frames)]),
]

parity = {}
for g in games:
    rows = {}
    for ename, mkcmd in engines:
        out = subprocess.check_output(mkcmd(g), text=True).strip()
        print(f"VERIFY {g:14s} {ename:12s}: {out}", flush=True)
        kv = dict(tok.split("=", 1) for tok in out.split() if "=" in tok)
        rows[ename] = kv
    ref = rows["om_a64linux"]
    for ename, kv in rows.items():
        for k in ("HERO_FRAME", "HERO_PPM_SHA256", "FINAL_PPM_SHA256", "STREAM_SHA256"):
            if kv[k] != ref[k]:
                print(f"PARITY FAIL {g} {ename} {k}: {kv[k]} != {ref[k]}", file=sys.stderr)
                sys.exit(1)
    # Cross-check raw PPM stdout sha256sum against HERO_PPM_SHA256
    import hashlib
    ppm = subprocess.check_output(["taskset", "-c", cpu, f"{root}/bin/{g}", "ppm"])
    got_ppm_sha = hashlib.sha256(ppm).hexdigest()
    assert len(ppm) == 691215, f"unexpected ppm size {len(ppm)}"
    assert got_ppm_sha == ref["HERO_PPM_SHA256"], f"ppm mismatch {got_ppm_sha} != {ref['HERO_PPM_SHA256']}"
    print(f"PASS PARITY {g}: 4-way bit-exact across {frames} frames ({frames*230400:,} px) + raw PPM ({len(ppm)} B) verified", flush=True)
    parity[g] = {
        "frames": frames,
        "hero_frame": int(ref["HERO_FRAME"]),
        "hero_ppm_sha256": ref["HERO_PPM_SHA256"],
        "final_ppm_sha256": ref["FINAL_PPM_SHA256"],
        "stream_sha256": ref["STREAM_SHA256"],
        "ppm_bytes": len(ppm),
        "verdict": "EXACT_PARITY",
    }

with open(f"{root}/docs/evidence/parity_spark.json", "w") as f:
    json.dump(parity, f, indent=2)
PYEOF

echo "=== [5/5] Paired Interleaved H2H FPS, Latency & Peak RAM (VmHWM) Benchmark ($TRIALS Trials, CPU $BENCH_CPU) ==="
python3 - "$ROOT" "$BENCH_CPU" "$FRAMES" "$TRIALS" << 'PYEOF'
import json, os, statistics, subprocess, sys, time

root, cpu, frames, trials = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
games = ["raycast_doom", "blocks_puzzle", "starfighter"]

def run_with_vmhwm(cmd):
    # Use os.wait4 on the direct child pinned via taskset; taskset execs the target binary,
    # so rusage.ru_maxrss reflects the target process's Linux kernel hiwater_rss (VmHWM in KiB).
    t0 = time.perf_counter_ns()
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    _, status, rusage = os.wait4(p.pid, 0)
    wall_ns = time.perf_counter_ns() - t0
    out = p.stdout.read().decode().strip()
    err = p.stderr.read().decode().strip()
    p.stdout.close()
    p.stderr.close()
    rc = os.waitstatus_to_exitcode(status)
    if rc != 0:
        raise RuntimeError(f"cmd failed rc={rc}: {cmd} stderr={err}")
    kv = dict(tok.split("=", 1) for tok in out.split() if "=" in tok)
    ns = int(kv["NS"])
    us_per_frame = ns / (frames * 1000.0)
    fps = (frames * 1e9) / ns if ns > 0 else 0.0
    vmhwm_kb = int(kv.get("VMHWM_KB", 0))
    if vmhwm_kb <= 0:
        vmhwm_kb = int(rusage.ru_maxrss)
    return {
        "ns": ns,
        "ms": round(ns / 1e6, 3),
        "wall_ms": round(wall_ns / 1e6, 3),
        "us_per_frame": round(us_per_frame, 2),
        "fps": round(fps, 2),
        "vmhwm_kb": vmhwm_kb,
        "anti_dce": kv["ANTI_DCE"],
        "final_ppm_sha256": kv["FINAL_PPM_SHA256"],
    }

engines = [
    ("om_a64linux", lambda g: ["taskset", "-c", cpu, f"{root}/bin/{g}", "bench", str(frames)]),
    ("c_gcc_o3",    lambda g: ["taskset", "-c", cpu, f"{root}/bin/c/{g}", "bench", str(frames)]),
    ("node_v24",    lambda g: ["taskset", "-c", cpu, "node", f"{root}/ref/node/{g}.js", "bench", str(frames)]),
    ("python3",     lambda g: ["taskset", "-c", cpu, "python3", f"{root}/ref/py/{g}.py", "bench", str(frames)]),
]

with open(f"{root}/docs/evidence/parity_spark.json") as f:
    parity = json.load(f)

results = {
    "cpu_pin": int(cpu),
    "frames": frames,
    "trials": trials,
    "binary_sizes_bytes": {
        g: {
            "om_a64linux": os.path.getsize(f"{root}/bin/{g}"),
            "c_gcc_o3": os.path.getsize(f"{root}/bin/c/{g}"),
        }
        for g in games
    },
    "gate_binary_bytes": os.path.getsize(f"{root}/bin/gate"),
    "parity": parity,
    "games": {},
}

for g in games:
    per_eng = {e[0]: [] for e in engines}
    for t in range(1, trials + 1):
        for ename, mkcmd in engines:
            r = run_with_vmhwm(mkcmd(g))
            assert r["final_ppm_sha256"] == parity[g]["final_ppm_sha256"], f"sha mismatch in bench {g} {ename}"
            per_eng[ename].append(r)
            print(
                f"TRIAL {t}/{trials} {g:14s} {ename:12s}: "
                f"{r['fps']:8.2f} FPS | {r['us_per_frame']:8.2f} us/f | "
                f"{r['ms']:8.2f} ms | VmHWM={r['vmhwm_kb']:6d} KB | DCE={r['anti_dce']}",
                flush=True,
            )
    # Assert identical ANTI_DCE across every engine and trial
    ref_dce = per_eng["om_a64linux"][0]["anti_dce"]
    summary = {}
    for ename, arr in per_eng.items():
        for r in arr:
            assert r["anti_dce"] == ref_dce, f"ANTI_DCE mismatch {g} {ename}: {r['anti_dce']} != {ref_dce}"
        summary[ename] = {
            "median_fps": round(statistics.median(r["fps"] for r in arr), 2),
            "median_us_per_frame": round(statistics.median(r["us_per_frame"] for r in arr), 2),
            "median_ms": round(statistics.median(r["ms"] for r in arr), 3),
            "median_vmhwm_kb": int(statistics.median(r["vmhwm_kb"] for r in arr)),
            "trials": arr,
        }
    om_fps = summary["om_a64linux"]["median_fps"]
    summary["speedups_vs_om"] = {
        ename: round(om_fps / summary[ename]["median_fps"], 2) if summary[ename]["median_fps"] > 0 else 0.0
        for ename in summary
    }
    results["games"][g] = summary

out_path = f"{root}/docs/evidence/results.json"
with open(out_path, "w") as f:
    json.dump(results, f, indent=2)
print(f"WROTE {out_path}")
PYEOF
