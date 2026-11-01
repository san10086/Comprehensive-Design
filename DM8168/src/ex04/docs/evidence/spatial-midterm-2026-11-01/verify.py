"""Reproduce existing spatial tests and inspect existing audio; Python stdlib only."""
import array
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
import wave

EVIDENCE = Path(__file__).resolve().parent
ROOT = EVIDENCE.parents[2]
TEST = ROOT / "tests" / "spatial"
OUT = TEST / "build-verification"
OUT.mkdir(parents=True, exist_ok=True)
HRIR = ROOT / "assets" / "hrir" / "original"
CC = Path(os.environ.get("CC") or shutil.which("gcc") or
          r"D:\develop\CLion 2025.1.2\bin\mingw\bin\gcc.exe")
ENV = os.environ.copy()
ENV["PATH"] = str(CC.parent) + os.pathsep + ENV.get("PATH", "")
log = []
commands = []


def run(args, expected=0):
    args = [str(a) for a in args]
    result = subprocess.run(args, cwd=TEST, env=ENV, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    command = subprocess.list2cmdline(args)
    record = {"cwd": str(TEST), "command": command,
              "returncode": result.returncode, "output": result.stdout}
    commands.append(record)
    log.extend(["CWD: " + str(TEST), "$ " + command, result.stdout.rstrip(),
                "EXIT: " + str(result.returncode), ""])
    print("$ " + command)
    print(result.stdout, end="")
    print("EXIT:", result.returncode)
    if result.returncode != expected:
        save_logs()
        raise RuntimeError("Unexpected return code")


def save_logs():
    (OUT / "commands.log").write_text("\n".join(log), encoding="utf-8")
    (OUT / "commands.json").write_text(json.dumps(commands, indent=2), encoding="utf-8")


def audio_info(path):
    data = path.read_bytes()
    info = {"path": str(path.relative_to(ROOT)), "bytes": len(data),
            "sha256": hashlib.sha256(data).hexdigest(),
            "header_hex": data[:32].hex(),
            "header_ascii": repr(data[:32])}
    try:
        with wave.open(str(path), "rb") as wav:
            info.update(channels=wav.getnchannels(), sample_width_bytes=wav.getsampwidth(),
                        sample_rate=wav.getframerate(), frames=wav.getnframes(),
                        compression=wav.getcomptype(),
                        duration_seconds=wav.getnframes() / wav.getframerate())
            frames = wav.readframes(wav.getnframes())
        info["pcm_sha256"] = hashlib.sha256(frames).hexdigest()
        if info["sample_width_bytes"] == 2:
            values = array.array("h", frames)
            if sys.byteorder != "little":
                values.byteswap()
            info["min_sample"] = min(values) if values else None
            info["max_sample"] = max(values) if values else None
            info["peak_abs_sample"] = max(abs(x) for x in values) if values else None
            info["rms_sample"] = math.sqrt(sum(x*x for x in values) / len(values)) if values else None
            info["full_scale_samples"] = sum(x in (-32768, 32767) for x in values)
    except (wave.Error, EOFError) as exc:
        info["wave_error"] = str(exc)
        info["iso_bmff_ftyp"] = data[4:8] == b"ftyp"
        info["major_brand_ascii"] = data[8:12].decode("ascii", errors="replace")
    return info


run([CC, "--version"])
flags = ["-std=c99", "-O2", "-Wall", "-Wextra", "-Werror", "-pedantic"]
q15 = OUT / "spatial_q15.exe"
dsp = OUT / "dsp_spatial_selftest.exe"
run([CC, *flags, "-o", q15, "main.c", "spatial_fir.c", "wav_pcm16.c"])
run([CC, *flags, "-o", dsp, "dsp_spatial_selftest.c", "spatial_fir.c",
     "wav_pcm16.c", "../../dsp/dsp_spatial.c"])
l40, r40, l60, r60 = [HRIR / name for name in
                     ("L40e045a.wav", "R40e045a.wav", "L60e250a.wav", "R60e250a.wav")]
run([q15, "selftest", l40, r40])
run([q15, "selftest", l60, r60])
run([dsp, l40, r40, l60, r60])
run([q15, "render", TEST / "input.wav", l40, r40, OUT / "rerendered_511.wav", "511"])
run([q15, "render", TEST / "input.wav", l40, r40, OUT / "rerendered_1024.wav", "1024"])
run([q15, "compare", OUT / "rerendered_511.wav", OUT / "rerendered_1024.wav", "0"])
run([q15, "compare", EVIDENCE / "rerendered_511.wav", EVIDENCE / "rerendered_1024.wav", "0"])
run([q15, "compare", OUT / "rerendered_511.wav", EVIDENCE / "rerendered_511.wav", "0"])
run([q15, "compare", OUT / "rerendered_1024.wav", EVIDENCE / "rerendered_1024.wav", "0"])
save_logs()

paths = sorted(HRIR.glob("*.wav")) + sorted(TEST.glob("*.wav")) + sorted(OUT.glob("*.wav"))
audio = [audio_info(path) for path in paths]
(OUT / "audio-inspection.json").write_text(json.dumps(audio, indent=2), encoding="utf-8")
rows = ["# Audio inspection (Python standard library)", "",
        "Inspection date: 2026-11-01 (Asia/Shanghai reporting date).", "",
        "| File | Rate | Channels | Bytes/sample | Frames | Duration (s) | Peak |", 
        "|---|---:|---:|---:|---:|---:|---:|"]
for info in audio:
    if "wave_error" in info:
        rows.append("| {} | {} | | | | | |".format(info["path"], info["wave_error"] +
                    "; ftyp=" + str(info["iso_bmff_ftyp"]) + "; brand=" + info["major_brand_ascii"]))
    else:
        rows.append("| {} | {} | {} | {} | {} | {:.6f} | {} |".format(
            info["path"], info["sample_rate"], info["channels"], info["sample_width_bytes"],
            info["frames"], info["duration_seconds"], info.get("peak_abs_sample", "n/a")))
rows += ["", "Detailed file and PCM hashes, headers, extrema, RMS and full-scale counts: `audio-inspection.json`.",
         "Only PC-side algorithm verification was performed. No board, latency measurement, listening panel, MATLAB runtime or deployment verification was performed."]
(OUT / "audio-inspection.md").write_text("\n".join(rows) + "\n", encoding="utf-8")
print("AUDIO INSPECTION:", OUT / "audio-inspection.json")
