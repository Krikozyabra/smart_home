#!/usr/bin/env python3
"""Exercise OWW, STT and TTS through the demo with local PCM and ONNX fixtures."""

from array import array
import os
from pathlib import Path
import select
import struct
import subprocess
import sys
import tempfile
import wave


FRAME_SAMPLES = 1280
PHRASE_SAMPLES = 16000
ALPHABET = "_~абвгдеёжзийклмнопрстуфхцчшщъыьэюя +.,!?…:;–"
ALSA_CONFIG = """
pcm.voice_null { type null }
pcm.voice_capture {
    type file
    slave.pcm "voice_null"
    file "/dev/null"
    infile 0
    format "raw"
}
pcm.voice_playback {
    type file
    slave.pcm "voice_null"
    file "reply.wav"
    format "wav"
}
"""


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def status(process, expected):
    readable, _, _ = select.select([process.stdout], [], [], 8)
    check(readable, f"Timed out waiting for {expected!r}")
    line = process.stdout.readline().decode("utf-8").rstrip()
    check(line.startswith(expected), f"Expected {expected!r}, got {line!r}")
    return line


def send_frames(process, samples):
    check(len(samples) % FRAME_SAMPLES == 0, "Expected complete capture frames")
    for offset in range(0, len(samples), FRAME_SAMPLES):
        # A complete frame fits in PIPE_BUF so the ALSA file plugin reads it intact.
        packet = samples[offset:offset + FRAME_SAMPLES].tobytes()
        check(os.write(process.stdin.fileno(), packet) == len(packet), "Short virtual capture write")


def signal():
    samples = array("h")
    state = 13
    for _ in range(PHRASE_SAMPLES):
        state = (state * 1664525 + 1013904223) & 0xFFFFFFFF
        samples.append((state >> 16) - 32768)
    return samples


def check_playback(path, expected_text):
    # The TTS echo fixture exposes text token IDs, speaker index and sample rate.
    indices = [ALPHABET.index(symbol) for symbol in expected_text]
    expected = [int(index * 32767 / 64 + 0.5) for index in indices]
    expected += [0, 16384, -32767, -32767, -16384, 0, 16384, 32767, 32767]
    with wave.open(str(path), "rb") as reply:
        check((reply.getnchannels(), reply.getsampwidth(), reply.getframerate()) == (1, 2, 24000),
              "Unexpected playback format")
        check(reply.getnframes() == len(expected), "Unexpected playback length")
        delivered = struct.unpack(f"<{len(expected)}h", reply.readframes(len(expected)))
        check(list(delivered) == expected, f"Unexpected reply tokens or speaker PCM: {delivered}")


def run_demo(command, phrase, transcript, reply):
    with tempfile.TemporaryDirectory(prefix="smart-home-voice-demo-") as directory:
        config = Path(directory) / "alsa.conf"
        config.write_text(ALSA_CONFIG, encoding="utf-8")
        env = dict(os.environ, ALSA_CONFIG_PATH=str(config))
        with subprocess.Popen(command, cwd=directory, env=env, stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0) as process:
            try:
                status(process, "Waiting for the wake word")
                send_frames(process, array("h", [0]) * (2 * FRAME_SAMPLES))
                send_frames(process, array("h", [20000]) * FRAME_SAMPLES)
                wake = status(process, "Wake word detected (probability ")
                check(float(wake.split("probability ")[1].split(")")[0]) >= 0.6, wake)

                phrase.extend(array("h", [0]) * (-len(phrase) % FRAME_SAMPLES))
                send_frames(process, phrase)
                status(process, f"Captured {PHRASE_SAMPLES} samples.")
                # Release the virtual capture's blocking input after the demo has its phrase.
                process.stdin.close()
                process.stdin = None
                output, errors = process.communicate(timeout=8)
                check(process.returncode == 0, repr(output + errors))
                check(output.decode("utf-8").splitlines() == [f"Transcript: {transcript}",
                                                                "Playing reply with Aidar..."], repr(output))
            finally:
                if process.poll() is None:
                    process.kill()
        check_playback(Path(directory) / "reply.wav", reply)


def main():
    demo, fixtures = sys.argv[1:]
    demo = str(Path(demo).resolve())
    fixtures = Path(fixtures).resolve()
    args = [demo, "--wake-model", str(fixtures / "oww/models/keyword_one_frame.onnx"),
            "--stt-model", str(fixtures / "stt/ctc.onnx"),
            "--stt-tokens", str(fixtures / "stt/tokens.txt"),
            "--tts-model", str(fixtures / "tts/models/echo.onnx"),
            "--capture-device", "voice_capture", "--playback-device", "voice_playback",
            "--record-seconds", "1", "--threshold", "0.6"]
    for flags, expected in [(["--help"], 0), ([], 1), (["--record-seconds", "0"], 1)]:
        result = subprocess.run([demo, *flags], capture_output=True, timeout=5)
        check(result.returncode == expected, repr(result.stdout + result.stderr))
    run_demo(args, signal(), "а", "повтор+ю. а.~")
    run_demo(args, array("h", [0]) * PHRASE_SAMPLES, "(empty)", "я не рассл+ышал.~")


if __name__ == "__main__":
    main()
