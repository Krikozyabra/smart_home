#!/usr/bin/env python3
"""Regenerate the tiny TTS ONNX fixtures using the standard library.

Wire fields follow https://github.com/onnx/onnx/blob/v1.16.0/onnx/onnx.proto.
"""

from pathlib import Path
import struct


def varint(value):
    data = bytearray()
    while value > 127:
        data.append((value & 127) | 128)
        value >>= 7
    data.append(value)
    return bytes(data)


def integer(field, value):
    return varint(field << 3) + varint(value)


def blob(field, value):
    if isinstance(value, str):
        value = value.encode()
    return varint(field << 3 | 2) + varint(len(value)) + value


def tensor(name, shape, values, dtype=1):
    raw = struct.pack("<" + ("f" if dtype == 1 else "q") * len(values), *values)
    return (b"".join(integer(1, dim) for dim in shape) + integer(2, dtype)
            + blob(8, name) + blob(9, raw))


def value_info(name, shape, dtype=1):
    dimensions = b"".join(blob(1, blob(2, dim) if isinstance(dim, str)
                               else integer(1, dim)) for dim in shape)
    return blob(1, name) + blob(2, blob(1, integer(1, dtype) + blob(2, dimensions)))


def node(op, inputs, output, **attrs):
    return (b"".join(blob(1, name) for name in inputs) + blob(2, output)
            + blob(4, op) + b"".join(blob(5, blob(1, name) + integer(3, value)
                                          + integer(20, 2)) for name, value in attrs.items()))


def model(*, speaker="speaker", speaker_type=7, length="tokens", mode="echo"):
    nodes = [node("Cast", ["input"], "numeric_tokens", to=1),
             node("Div", ["numeric_tokens", "token_scale"], "normalized_tokens"),
             node("Cast", [speaker], "speaker_float", to=1),
             node("Div", ["speaker_float", "speaker_scale"], "speaker_scaled"),
             node("Unsqueeze", ["speaker_scaled", "axis"], "speaker_row"),
             node("Cast", ["sample_rate"], "rate_float", to=1),
             node("Div", ["rate_float", "rate_scale"], "rate_scaled"),
             node("Unsqueeze", ["rate_scaled", "axis"], "rate_row")]
    constants = [tensor("token_scale", [], [64]), tensor("speaker_scale", [], [8]),
                 tensor("rate_scale", [], [48000]), tensor("axis", [1], [0], 7)]
    rows = ["normalized_tokens", "speaker_row", "rate_row", "edge_samples"]
    constants.append(tensor("edge_samples", [1, 7], [-2, -1, -0.5, 0, 0.5, 1, 2]))
    output_shape = [1, "samples"]

    if mode == "warm_failure":
        constants.append(tensor("nan", [1, 1], [float("nan")]))
        rows.append("nan")
    elif mode == "late_failure":
        constants.extend([tensor("first", [1], [0], 7), tensor("ya", [], [34], 7),
                          tensor("yo", [], [8], 7), tensor("nan", [], [float("nan")]),
                          tensor("inf", [], [float("inf")]), tensor("zero", [], [0])])
        nodes.extend([node("Gather", ["input", "first"], "first_token", axis=1),
                      node("Equal", ["first_token", "ya"], "is_ya"),
                      node("Equal", ["first_token", "yo"], "is_yo"),
                      node("Where", ["is_yo", "inf", "zero"], "yo_sample"),
                      node("Where", ["is_ya", "nan", "yo_sample"], "conditional")])
        rows.append("conditional")
    elif mode == "run_error":
        # A three-token input (one letter, punctuation and EOF) is too short.
        constants.append(tensor("index", [1], [3], 7))
        nodes.append(node("Gather", ["normalized_tokens", "index"], "fourth", axis=1))
        rows.append("fourth")
    elif mode == "rank_one":
        output_shape = ["samples"]
        nodes.append(node("Concat", rows, "row", axis=1))
        nodes.append(node("Squeeze", ["row", "axis"], "output"))

    if mode != "rank_one":
        nodes.append(node("Concat", rows, "output", axis=1))

    graph = (b"".join(blob(1, item) for item in nodes) + blob(2, "tts_test_graph")
             + b"".join(blob(5, item) for item in constants)
             + blob(11, value_info("sample_rate", [1], 7))
             + blob(11, value_info("input", [1, length], 7))
             + blob(11, value_info(speaker, [1], speaker_type))
             + blob(12, value_info("output", output_shape)))
    return integer(1, 8) + blob(2, "smart_home_tts_tests") + blob(7, graph) + blob(8, integer(2, 13))


def main():
    directory = Path(__file__).parent / "models"
    directory.mkdir(exist_ok=True)
    fixtures = {
        "echo": model(),
        "wrong_name": model(speaker="speaker_id"),
        "wrong_type": model(speaker_type=1),
        "fixed_input": model(length=10),
        "wrong_rank": model(mode="rank_one"),
        "warm_failure": model(mode="warm_failure"),
        "late_failure": model(mode="late_failure"),
        "run_error": model(mode="run_error"),
    }
    for name, data in fixtures.items():
        (directory / (name + ".onnx")).write_bytes(data)


if __name__ == "__main__":
    main()
