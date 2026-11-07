#!/usr/bin/env python3
"""Console integration against ALSA null and local ONNX fixtures; no hardware/network."""
import os
from pathlib import Path
import selectors
import subprocess
import sys
import tempfile
import time

app, models = sys.argv[1:]


def check(condition, message):
    if not condition:
        raise AssertionError(message)


with tempfile.TemporaryDirectory(prefix="audio-oww-console-") as directory:
    config = Path(directory) / "alsa.conf"
    config.write_text("pcm.demo_null { type null }\n")
    env = dict(os.environ, ALSA_CONFIG_PATH=str(config))
    base = [app, "--device", "demo_null", "--model", str(Path(models) / "keyword.onnx")]

    def invoke(args, commands=""):
        return subprocess.run(args, input=commands, text=True, capture_output=True,
                              env=env, timeout=15)

    for threshold, expected in [("0.0001", "1"), ("1", "0")]:
        result = invoke(base + ["--threshold", threshold], "sample\nquit\n")
        check(result.returncode == 0, result.stdout + result.stderr)
        check("Recorded 80000 samples" in result.stdout, result.stdout)
        check("Sample summary: frames=63" in result.stdout, result.stdout)
        check(f"detections={expected}" in result.stdout, result.stdout)
        check(("WAKE WORD detected" in result.stdout) == (expected == "1"), result.stdout)

    # Read readiness/events instead of sleeping: prove processing continues while
    # the console waits for input, then exercise conflicting commands and restart.
    process = subprocess.Popen(base + ["--threshold", "0.0001"], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    output = b""
    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ)

    def wait_for(fragment, after=0):
        global output
        deadline = time.monotonic() + 15
        while fragment not in output[after:]:
            remaining = deadline - time.monotonic()
            check(remaining > 0, repr(output))
            check(selector.select(remaining), repr(output))
            chunk = os.read(process.stdout.fileno(), 65536)
            check(chunk, repr(output))
            output += chunk

    def send(command):
        process.stdin.write(command.encode())
        process.stdin.flush()

    try:
        wait_for(b"Commands:")
        send("start\n")
        wait_for(b"WAKE WORD detected")
        send("start\nsample\nstop\nstop\nunknown\n")
        wait_for(b"Unknown command:")
        check(b"Already recording." in output, repr(output))
        check(b"Stop live recording before sample." in output, repr(output))
        check(b"Live summary: frames=" in output, repr(output))
        check(b"Recording is stopped." in output, repr(output))
        mark = len(output)
        send("start\n")
        wait_for(b"WAKE WORD detected", mark)
        process.stdin.close()  # EOF must stop/join a running microphone.
        process.wait(timeout=15)
        output += process.stdout.read()
        errors = process.stderr.read()
        check(process.returncode == 0, repr(output + errors))
        check(output.count(b"Live summary:") == 2, repr(output))
    finally:
        selector.close()
        if process.poll() is None:
            process.kill()
            process.wait()

    # CLI rejection occurs without a real device or downloading any models.
    for threshold in ("nan", "inf", "0", "-1", "1.1", "0.5junk"):
        check(invoke(base + ["--threshold", threshold]).returncode != 0, threshold)
    check(invoke([app, "--help"]).returncode == 0, "--help")
    check(invoke([app]).returncode != 0, "missing model")
    check(invoke(base + ["--device", "missing_device"]).returncode != 0, "missing device")
    check(invoke(base + ["--model", str(Path(models) / "keyword_bad_width.onnx")],
                 "sample\n").returncode != 0, "incompatible model")
    # Overrides must work even when the classifier's adjacent models are absent.
    classifier = Path(directory) / "classifier.onnx"
    classifier.write_bytes((Path(models) / "keyword.onnx").read_bytes())
    result = invoke(base + ["--model", str(classifier),
                           "--melspec", str(Path(models) / "melspectrogram.onnx"),
                           "--embedding", str(Path(models) / "embedding_model.onnx")],
                    "help\nquit")  # final command without newline
    check(result.returncode == 0, result.stdout + result.stderr)

print("Audio/OWW console integration passed")
