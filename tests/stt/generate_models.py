#!/usr/bin/env python3
"""Generate small GigaAM-shaped CTC ONNX fixtures using the Python stdlib."""

from pathlib import Path
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "oww"))
from generate_models import attr_int, blob, integer, node, tensor, value_info


def model(feature_dim=64, include_vocab=True, runtime_error=False):
    nodes = [
        node("ReduceMean", ["features"], "mean", [attr_int("keepdims", 0)]),
        node("Greater", ["mean", "low"], "audible"),
        node("Less", ["mean", "high"], "normalized"),
        node("And", ["audible", "normalized"], "speech"),
        node("Where", ["speech", "spoken", "blank"], "frame_logits"),
    ]
    tensors = [
        tensor("low", [], [-12.0]),
        tensor("high", [], [12.0]),
        tensor("spoken", [1, 1, 3], [0.0, 2.0, -1.0]),
        tensor("blank", [1, 1, 3], [0.0, -1.0, 2.0]),
        tensor("batch", [1], [1], dtype=7),
        tensor("vocab", [1], [3], dtype=7),
    ]
    frames = "length"
    if runtime_error:
        # Warmup has 10 frames. A later long clip goes out of bounds in Gather.
        tensors += [tensor("divisor", [1], [16], dtype=7),
                    tensor("zero", [1], [0], dtype=7)]
        nodes += [node("Div", ["length", "divisor"], "bucket"),
                  node("Gather", ["zero", "bucket"], "offset"),
                  node("Add", ["length", "offset"], "output_frames")]
        frames = "output_frames"
    nodes += [
        node("Concat", ["batch", frames, "vocab"], "shape", [attr_int("axis", 0)]),
        node("Expand", ["frame_logits", "shape"], "logits"),
    ]

    graph = (b"".join(blob(1, entry) for entry in nodes)
             + blob(2, "stt_test")
             + b"".join(blob(5, entry) for entry in tensors)
             + blob(11, value_info("features", [1, feature_dim, "frames"]))
             + blob(11, value_info("length", [1], dtype=7))
             + blob(12, value_info("logits", [1, "output_frames", 3])))
    metadata = {
        "model_type": "EncDecCTCModel",
        "is_giga_am": "1",
        "normalize_type": "",
        "subsampling_factor": "4",
    }
    if include_vocab:
        metadata["vocab_size"] = "3"
    return (integer(1, 8) + blob(7, graph) + blob(8, integer(2, 13))
            + b"".join(blob(14, blob(1, key) + blob(2, value))
                       for key, value in metadata.items()))


if __name__ == "__main__":
    root = Path(__file__).parent
    for name, data in {
        "ctc.onnx": model(),
        "missing_metadata.onnx": model(include_vocab=False),
        "wrong_feature_dim.onnx": model(feature_dim=63),
        "runtime_error.onnx": model(runtime_error=True),
    }.items():
        (root / name).write_bytes(data)
