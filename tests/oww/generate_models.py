#!/usr/bin/env python3
"""Regenerate tiny ONNX test graphs using only the Python standard library.

These graphs test data flow, not wake-word recognition. The production module
always uses ONNX Runtime. Wire field numbers follow the official ONNX schema:
https://github.com/onnx/onnx/blob/v1.16.0/onnx/onnx.proto
The generated files are checked in, so building/tests do not require Python.
"""

from pathlib import Path
import struct


def varint(value):
    value &= (1 << 64) - 1
    result = bytearray()
    while value > 127:
        result.append((value & 127) | 128)
        value >>= 7
    result.append(value)
    return bytes(result)


def integer(number, value):
    return varint(number << 3) + varint(value)


def blob(number, value):
    if isinstance(value, str):
        value = value.encode()
    return varint((number << 3) | 2) + varint(len(value)) + value


def tensor(name, shape, values, dtype=1):
    raw = struct.pack("<" + ("f" if dtype == 1 else "q") * len(values), *values)
    return (b"".join(integer(1, dim) for dim in shape) + integer(2, dtype)
            + blob(8, name) + blob(9, raw))


def value_info(name, shape, dtype=1):
    dimensions = b"".join(blob(1, blob(2, dim) if isinstance(dim, str)
                               else integer(1, dim)) for dim in shape)
    tensor_type = integer(1, dtype) + blob(2, dimensions)
    return blob(1, name) + blob(2, blob(1, tensor_type))


def node(op, inputs, output, attributes=()):
    return (b"".join(blob(1, name) for name in inputs) + blob(2, output)
            + blob(4, op) + b"".join(blob(5, attr) for attr in attributes))


def attr_int(name, value):
    return blob(1, name) + integer(3, value) + integer(20, 2)


def mean_nodes():
    # Double accumulation keeps the analytic oracle portable across ORT kernels;
    # production-facing tensor boundaries remain float32, like OpenWakeWord.
    return [node("Cast", ["test_input"], "input64", [attr_int("to", 11)]),
            node("ReduceMean", ["input64"], "mean64", [attr_int("keepdims", 0)]),
            node("Cast", ["mean64"], "mean", [attr_int("to", 1)])]


def model(input_shape, output_shape, nodes, initializers, dtype=1, extra_output=False):
    graph = (b"".join(blob(1, item) for item in nodes) + blob(2, "oww_test_graph")
             + b"".join(blob(5, item) for item in initializers)
             + blob(11, value_info("test_input", input_shape, dtype))
             + blob(12, value_info("test_output", output_shape, dtype)))
    if extra_output:
        graph += blob(12, value_info("mean", [], dtype))
    return (integer(1, 8) + blob(2, "smart_home_oww_tests") + blob(7, graph)
            + blob(8, integer(2, 13)))


def mean_expand(input_shape, output_shape, nonfinite=False):
    nodes = mean_nodes()
    initializers = [tensor("shape", [len(output_shape)], output_shape, 7)]
    result = "mean"
    if nonfinite:
        initializers += [tensor("threshold", [], [2.01]), tensor("infinity", [], [float("inf")])]
        nodes += [node("Greater", ["mean", "threshold"], "bad"),
                  node("Where", ["bad", "infinity", "mean"], "checked")]
        result = "checked"
    nodes += [node("Expand", [result, "shape"], "test_output")]
    return model(input_shape, output_shape, nodes, initializers)


def classifier(input_shape=(1, 16, 96), output_shape=(1, 1), mode="sigmoid", extra_output=False):
    nodes = mean_nodes() + [node("Sub", ["mean", "bias"], "centered"),
                            node("Div", ["centered", "scale"], "scaled")]
    initializers = [tensor("bias", [], [2]), tensor("scale", [], [100]),
                    tensor("shape", [len(output_shape)], output_shape, 7)]
    if mode == "sigmoid":
        nodes += [node("Sigmoid", ["scaled"], "score")]
    elif mode == "linear":
        initializers += [tensor("half", [], [0.5])]
        nodes += [node("Add", ["scaled", "half"], "score")]
    else:
        initializers += [tensor("threshold", [], [2.01]), tensor("half", [], [0.5]),
                         tensor("invalid", [], [float(mode)])]
        nodes += [node("Greater", ["mean", "threshold"], "bad"),
                  node("Where", ["bad", "invalid", "half"], "score")]
    nodes += [node("Expand", ["score", "shape"], "test_output")]
    return model(input_shape, output_shape, nodes, initializers, extra_output=extra_output)


def main():
    directory = Path(__file__).parent / "models"
    directory.mkdir(exist_ok=True)
    graphs = {
        "melspectrogram": mean_expand([1, "samples"], [1, 1, 8, 32]),
        "embedding_model": mean_expand(["batch", 76, 32, 1], [1, 96]),
        "keyword": classifier(),
        "keyword_one_frame": classifier([1, 1, 96]),
        "keyword_max_context": classifier([1, 120, 96]),
        "keyword_bad_width": classifier([1, 16, 95]),
        "keyword_dynamic_context": classifier([1, "frames", 96]),
        "keyword_zero_context": classifier([1, 0, 96]),
        "keyword_large_context": classifier([1, 121, 96]),
        "keyword_multiclass": classifier(output_shape=[1, 2]),
        "keyword_extra_output": classifier(extra_output=True),
        "keyword_linear": classifier(mode="linear"),
        "keyword_nan": classifier(mode="nan"),
        "keyword_inf": classifier(mode="inf"),
        "keyword_bad_type": model([1, 16, 96], [1, 16, 96],
                                  [node("Identity", ["test_input"], "test_output")], [], dtype=7),
        "mel_bad_input": mean_expand([1, 1280], [1, 1, 8, 32]),
        "mel_bad_output": mean_expand([1, 1760], [1, 1, 8, 31]),
        "mel_bad_layout": mean_expand([1, 1760], [1, 1, 32, 8]),
        "mel_nonfinite": mean_expand([1, "samples"], [1, 1, 8, 32], nonfinite=True),
        "embedding_bad_input": mean_expand([1, 76, 32, 2], [1, 96]),
        "embedding_bad_output": mean_expand([1, 76, 32, 1], [1, 95]),
        "embedding_bad_layout": mean_expand([1, 76, 32, 1], [1, 12, 8]),
        "embedding_nonfinite": mean_expand([1, 76, 32, 1], [1, 96], nonfinite=True),
    }
    for name, data in graphs.items():
        (directory / (name + ".onnx")).write_bytes(data)
    print(f"Generated {len(graphs)} ONNX test models in {directory}")


if __name__ == "__main__":
    main()
