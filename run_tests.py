#!/usr/bin/env python3
"""Host test runner: compiles portable modules with gcc and runs them."""
import subprocess, sys, os, glob

ROOT = os.path.dirname(os.path.abspath(__file__))
CC = os.environ.get("CC", "gcc")
CFLAGS = ["-std=c99", "-Wall", "-Wextra", "-I", os.path.join(ROOT, "include")]

PORTABLE = [
    "source/lyrics/lrc_parser.c",
    "source/lyrics/lyric_timeline.c",
    "source/library/metadata.c",
    "source/library/music_library.c",
    "source/app/playback_controller.c",
    "source/audio/mp3_decoder.c",
    "source/audio/audio_stream.c",
    "source/storage/config.c",
]

HELIX = sorted(
    ["third_party/helix/mp3dec.c", "third_party/helix/mp3tabs.c"] +
    [p.replace(ROOT + os.sep, "").replace("\\", "/")
     for p in glob.glob(os.path.join(ROOT, "third_party", "helix", "*.c"))
     if os.path.basename(p) not in ("mp3dec.c", "mp3tabs.c")]
)

TESTS = {
    "test_lrc": {"srcs": ["tests/test_lrc.c", "source/lyrics/lrc_parser.c"]},
    "test_timeline": {"srcs": ["tests/test_timeline.c"] + PORTABLE},
    "test_lyric_text": {"srcs": ["tests/test_lyric_text.c", "source/lyrics/lyric_text.c"]},
    "test_resample": {"srcs": ["tests/test_resample.c"]},
    "test_decode": {
        "srcs": ["tests/test_decode.c", "source/audio/mp3_decoder.c"] + HELIX,
        "flags": ["-DUSE_HELIX", "-U_WIN32",
                  "-I", os.path.join(ROOT, "third_party", "helix"),
                  "-Wno-unused-parameter", "-Wno-sign-compare",
                  "-Wno-unused-function", "-Wno-unused-but-set-variable"],
    },
}

def run(cmd):
    print("+ " + " ".join(cmd))
    r = subprocess.run(cmd, cwd=ROOT)
    return r.returncode

def main():
    os.makedirs(os.path.join(ROOT, "build", "host"), exist_ok=True)
    fails = 0
    for name, t in TESTS.items():
        exe = os.path.join(ROOT, "build", "host", name + (".exe" if os.name == "nt" else ""))
        rc = run([CC] + CFLAGS + t.get("flags", []) +
                 [os.path.join(ROOT, s) for s in t["srcs"]] +
                 (["-lm"] if os.name != "nt" else []) + ["-o", exe])
        if rc != 0:
            print(f"COMPILE FAIL: {name}")
            fails += 1
            continue
        rc = run([exe])
        if rc != 0:
            print(f"TEST FAIL: {name}")
            fails += 1
        else:
            print(f"PASS: {name}")
    if fails:
        print(f"{fails} suite(s) FAILED")
        return 1
    print("All host test suites passed.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
