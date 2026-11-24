#!/usr/bin/env python3
"""Export the waveform path from the SHA-256-pinned Silero v4_ru package.

The C++ module supplies normalized text tokens and explicit stress markers.
The exported graph uses the checkpoint's weights and all three vocoder rates.
"""

import argparse
import math

import numpy as np
import onnx
import onnxruntime as ort
import torch
from torch import Tensor, nn
from torch.nn import functional as F


ALPHABET = "_~абвгдеёжзийклмнопрстуфхцчшщъыьэюя +.,!?…:;–"
RANDOM_VOICE = 5
INPUT_NAMES = ("input", "speaker", "sample_rate")


class AttentionBlock(nn.Module):
    """Use dense tensor ops for the checkpoint's TorchScript attention."""

    def __init__(self, block):
        super().__init__()
        attention = block.self_attn
        self.heads = attention.num_heads
        self.head_size = attention.head_dim
        self.scale = 1 / math.sqrt(self.head_size)
        self.register_buffer("weight", attention.in_proj_weight.detach())
        self.register_buffer("bias", attention.in_proj_bias.detach())
        self.out_proj = attention.out_proj
        self.conv1 = block.conv1
        self.conv2 = block.conv2
        self.norm1 = block.norm1
        self.norm2 = block.norm2
        self.linear = block.use_linear

    def forward(self, x: Tensor, mask: Tensor) -> Tensor:
        mixed = F.linear(x, self.weight, self.bias)
        heads = mixed.reshape(x.size(0), x.size(1), 3, self.heads, self.head_size)
        q, k, v = heads.permute(2, 0, 3, 1, 4).unbind(0)
        scores = (q * self.scale) @ k.transpose(-2, -1)
        weights = torch.softmax(scores.masked_fill(mask[:, None, None, :], float("-inf")), dim=-1)
        attended = (weights @ v).transpose(1, 2).reshape_as(x)
        x = self.norm1(x + self.out_proj(attended))
        if self.linear:
            return self.norm2(x + self.conv2(F.relu(self.conv1(x))))
        channels_first = x.transpose(1, 2)
        return self.norm2(x + self.conv2(F.relu(self.conv1(channels_first))).transpose(1, 2))


class Transformer(nn.Module):
    def __init__(self, model):
        super().__init__()
        self.position = model.pos_encoder
        self.blocks = nn.ModuleList(AttentionBlock(block) for block in model.layers.children())
        self.norm = model.norm

    def forward(self, x: Tensor, mask: Tensor) -> Tensor:
        x = self.position(x.transpose(0, 1)).transpose(0, 1)
        for block in self.blocks:
            x = block(x, mask)
        return self.norm(x)


class Decoder(nn.Module):
    def __init__(self, model):
        super().__init__()
        self.factor = model.shorten_factor
        self.position = model.pos_encoder
        self.pre = AttentionBlock(next(model.pre_vanilla_layers.children()))
        # The v4_ru checkpoint feeds the pooled signal directly to the last block.
        self.shortened_block = AttentionBlock(list(model.shorten_layers.children())[-1])
        self.post = AttentionBlock(next(model.post_vanilla_layers.children()))
        self.pool = model.downsample.avg_pool
        self.upsample = model.upsample.proj
        self.norm = model.norm

    def forward(self, x: Tensor) -> Tensor:
        mask = torch.zeros_like(x[:, :, 0], dtype=torch.bool)
        x = self.pre(self.position(x.transpose(0, 1)).transpose(0, 1), mask)
        length = x.size(1)
        padded = F.pad(x, (0, 0, 0, (-length) % self.factor))
        shortened = self.pool(padded.transpose(1, 2)).transpose(1, 2)
        short_mask = torch.zeros_like(shortened[:, :, 0], dtype=torch.bool)
        shortened = self.shortened_block(shortened, short_mask)
        expanded = self.upsample(shortened).reshape(x.size(0), -1, x.size(2))
        return self.norm(self.post((padded + expanded)[:, :length], mask))


def voice_embedding(embedding: Tensor, speaker: Tensor, random_id: int) -> Tensor:
    # Silero's random voice draws independent standard normal embeddings for
    # duration, pitch and acoustic prediction. The graph draws them per call.
    return torch.where(speaker[:, None] == random_id, torch.randn_like(embedding), embedding)


class Predictor(nn.Module):
    def __init__(self, model):
        super().__init__()
        self.embed = model.embedding
        self.voice = model.speaker_embedding
        self.transform = Transformer(model.transformer)
        self.out = model.lin
        self.random_id = RANDOM_VOICE

    def forward(self, sequence: Tensor, speaker: Tensor, mask: Tensor) -> Tensor:
        voice = voice_embedding(self.voice(speaker), speaker, self.random_id)
        return self.out(self.transform(self.embed(sequence) + voice.unsqueeze(1), mask))


def expand_durations(encoded: Tensor, durations: Tensor) -> Tensor:
    # A difference array maps each audio frame to its token in linear time,
    # also when several zero-duration tokens start at the same frame.
    ends = durations.cumsum(0)
    frames = torch.arange(ends[-1] + 1, dtype=torch.long)
    boundaries = torch.zeros_like(frames).scatter_add(0, ends - durations, torch.ones_like(durations))
    return encoded.index_select(1, boundaries[:-1].cumsum(0) - 1)


class SileroExport(nn.Module):
    def __init__(self, model, lookup, start):
        super().__init__()
        self.register_buffer("lookup", lookup)
        self.start = start
        self.duration = Predictor(model.dur_predictor.dur_pred)
        self.pitch = Predictor(model.pitch_predictor.pitch_pred)
        acoustic = model.tacotron
        self.embed = acoustic.embedding
        self.voice = acoustic.speaker_embedding
        self.encoder = Transformer(acoustic.encoder)
        self.pitch_proj = acoustic.pitch_proj
        self.pitch_strength = acoustic.pitch_strength
        self.decode = Decoder(acoustic.decoder)
        self.mel = acoustic.lin
        self.vocoder = model.vocoder
        self.random_id = RANDOM_VOICE

    def forward(self, input: Tensor, speaker: Tensor, sample_rate: Tensor) -> Tensor:
        start = torch.full_like(speaker.reshape(1, 1), self.start)
        sequence = torch.cat((start, self.lookup[input]), dim=1)
        mask = sequence == 0
        durations = (self.duration(sequence, speaker, mask).reshape(-1).exp() - 1).clamp_min(0).round().long()
        pitch = self.pitch(sequence, speaker, mask).transpose(1, 2)
        voice = voice_embedding(self.voice(speaker), speaker, self.random_id)
        encoded = self.encoder(self.embed(sequence), mask) + voice.unsqueeze(1)
        encoded = encoded + self.pitch_proj(pitch).transpose(1, 2) * self.pitch_strength
        mel = self.mel(self.decode(expand_durations(encoded, durations))).transpose(1, 2)
        if sample_rate[0] == 48000:
            audio = self.vocoder._forward_u(mel)
        else:
            audio = self.vocoder._forward_24(mel)
            if sample_rate[0] == 8000:
                audio = self.vocoder.pqmf_3(audio)[:, :1, :]
        return audio.squeeze(1)


def encode(text):
    return torch.tensor([[ALPHABET.index(symbol) for symbol in text + "~"]])


def verify_waveforms(path, package, original):
    onnx.checker.check_model(path)
    options = ort.SessionOptions()
    options.intra_op_num_threads = 2
    session = ort.InferenceSession(path, sess_options=options, providers=["CPUExecutionProvider"])
    for text, speaker in (("пров+ерка.", 0), ("прив+ет, м+ир!", 3)):
        reference_input, _ = original.preprocess_tacotron(package.symb_to_ascii(text))
        speaker_tensor = torch.tensor([speaker])
        for rate in (8000, 24000, 48000):
            inputs = (encode(text), speaker_tensor, torch.tensor([rate]))
            expected, _ = original.tts_model(reference_input.unsqueeze(0), speaker_tensor, rate)
            actual, = session.run(["output"], {name: value.numpy() for name, value in zip(INPUT_NAMES, inputs)})
            np.testing.assert_allclose(actual, expected.numpy(), atol=3e-4, rtol=1e-4, equal_nan=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("checkpoint")
    parser.add_argument("output")
    args = parser.parse_args()
    torch.set_num_threads(2)
    package = torch.package.PackageImporter(args.checkpoint).load_pickle("tts_models", "model")
    original = package.models[0].eval()
    lookup = torch.tensor([original.symbol_to_id[package.symb_ascii_dict.get(symbol, symbol)]
                           for symbol in ALPHABET])
    start = original.symbol_to_id[original.sos_token]
    module = torch.jit.freeze(torch.jit.script(SileroExport(original.tts_model, lookup, start).eval()))
    inputs = (encode("пров+ерка."), torch.tensor([3]), torch.tensor([24000]))
    with torch.no_grad():
        torch.onnx.export(module, inputs, args.output, input_names=list(INPUT_NAMES),
                          output_names=["output"], dynamic_axes={"input": {1: "tokens"}, "output": {1: "samples"}},
                          opset_version=17, dynamo=False, external_data=False)
        verify_waveforms(args.output, package, original)


if __name__ == "__main__":
    main()
