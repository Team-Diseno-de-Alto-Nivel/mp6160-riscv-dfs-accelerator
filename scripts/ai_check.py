#!/usr/bin/env python3
"""AI-writing pre-check for the LaTeX paper.

Extracts plain prose from a .tex file and scores it with a free, open-source
AI-text detector that runs locally (desklib/ai-text-detector, a DeBERTa-v3
model). Prints a Markdown report and exits non-zero when the document-level AI
probability exceeds AI_THRESHOLD, so it can block a CI check. No API key, no
quota, no cost.

IMPORTANT: this is NOT Turnitin and does NOT reproduce Turnitin's numbers.
Turnitin exposes no public API; this is an early-warning proxy only. Treat the
score as a signal, not a verdict (Turnitin says the same about its own report).

Usage:
    [AI_THRESHOLD=0.40] python scripts/ai_check.py docs/paper/main.tex

The gating metric is the AI-flagged *share* of the document (fraction of the
text the model attributes to AI), to mirror Turnitin's "% AI" number — the
check fails when that share exceeds AI_THRESHOLD (default 0.20 == the
professor's <20% rule).

Environment:
    AI_THRESHOLD      (optional)  Max AI share before failing. Default 0.20.
    AI_SENT_THRESHOLD (optional)  Per-sentence AI cutoff. Default 0.50.
    AI_TOP_N          (optional)  How many suspicious sentences to list. Default 10.
    AI_MODEL          (optional)  HF model id. Default desklib/ai-text-detector-v1.01.
    AI_REPORT_FILE    (optional)  Write the Markdown report to this path.
"""

from __future__ import annotations

import os
import re
import subprocess
import sys
import textwrap

MODEL_DIR = os.environ.get("AI_MODEL", "desklib/ai-text-detector-v1.01")
MAX_TOKENS = 768          # model context window used per inference
BATCH_SIZE = 8            # sentences scored per forward pass
MIN_SENTENCE_CHARS = 25   # ignore tiny fragments when ranking
# Per-sentence decision: prob above this counts the sentence as AI-written.
SENT_DECISION = float(os.environ.get("AI_SENT_THRESHOLD", "0.50"))
# Document gate: fail when the AI-flagged SHARE of the text exceeds this.
# Mirrors Turnitin's "% of the document detected as AI" — the professor's <20%.
DEFAULT_THRESHOLD = 0.20
DEFAULT_TOP_N = 10


# --------------------------------------------------------------------------- #
# Text extraction
# --------------------------------------------------------------------------- #
def extract_prose(tex_path: str) -> str:
    """Return clean prose from a .tex file.

    Prefers pandoc (best LaTeX->text fidelity); falls back to a regex stripper
    if pandoc is missing or chokes on something like a tikzpicture.
    """
    try:
        result = subprocess.run(
            ["pandoc", "-f", "latex", "-t", "plain", "--quiet", tex_path],
            capture_output=True,
            text=True,
            timeout=120,
        )
        if result.returncode == 0 and result.stdout.strip():
            return _normalize(result.stdout)
        sys.stderr.write(
            "pandoc failed or produced empty output; "
            f"falling back to regex stripper.\n{result.stderr}\n"
        )
    except FileNotFoundError:
        sys.stderr.write("pandoc not found; falling back to regex stripper.\n")
    except subprocess.TimeoutExpired:
        sys.stderr.write("pandoc timed out; falling back to regex stripper.\n")

    with open(tex_path, "r", encoding="utf-8") as fh:
        return _strip_latex(fh.read())


def _strip_latex(tex: str) -> str:
    """Crude LaTeX->prose fallback used only when pandoc is unavailable."""
    # Keep only the document body.
    body = re.search(r"\\begin\{document\}(.*?)\\end\{document\}", tex, re.DOTALL)
    tex = body.group(1) if body else tex
    # Drop noisy environments wholesale.
    for env in ("figure", "table", "tikzpicture", "thebibliography", "equation", "align"):
        tex = re.sub(rf"\\begin\{{{env}\*?\}}.*?\\end\{{{env}\*?\}}", " ", tex, flags=re.DOTALL)
    tex = re.sub(r"%.*", "", tex)                  # comments
    tex = re.sub(r"\$[^$]*\$", " ", tex)           # inline math
    # Accent escapes (\'u, \"o, \^e, \~n, ...) -> keep the base letter.
    tex = re.sub(r"\\[`'^\"~=.]\{?([a-zA-Z])\}?", r"\1", tex)
    tex = tex.replace("\\\\", "\n")                # line breaks
    # Commands taking a braced arg we don't want as text (refs/styling metadata).
    tex = re.sub(r"\\(bibliographystyle|bibliography|label|ref|cite\w*|input|include)\b\s*\{[^}]*\}", " ", tex)
    tex = re.sub(r"\\[a-zA-Z]+\*?(\[[^\]]*\])?", " ", tex)  # remaining commands + optional args
    tex = re.sub(r"[{}]", " ", tex)                # braces
    return _normalize(tex)


def _normalize(text: str) -> str:
    text = re.sub(r"[ \t]+", " ", text)
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip()


def split_sentences(text: str) -> list[str]:
    """Split prose into sentences on terminal punctuation (good enough for
    English academic prose; avoids an nltk dependency)."""
    flat = re.sub(r"\s+", " ", text).strip()
    parts = re.split(r"(?<=[.!?])\s+(?=[A-Z(])", flat)
    return [p.strip() for p in parts if len(p.strip()) >= 3]


# --------------------------------------------------------------------------- #
# Detector (local HF model). Encapsulated so it can be swapped.
# --------------------------------------------------------------------------- #
_MODEL = None
_TOKENIZER = None
_DEVICE = None


def _load_model():
    """Lazily load the desklib detector (custom DeBERTa-v3 head)."""
    global _MODEL, _TOKENIZER, _DEVICE
    if _MODEL is not None:
        return

    import torch
    import torch.nn as nn
    from transformers import AutoConfig, AutoTokenizer, PreTrainedModel
    from transformers.models.deberta_v2.modeling_deberta_v2 import DebertaV2Model

    class DesklibAIDetectionModel(PreTrainedModel):
        config_class = AutoConfig

        def __init__(self, config):
            super().__init__(config)
            self.model = DebertaV2Model(config)
            self.classifier = nn.Linear(config.hidden_size, 1)
            self.init_weights()

        def forward(self, input_ids, attention_mask=None, **_):
            outputs = self.model(input_ids, attention_mask=attention_mask)
            last_hidden = outputs[0]
            mask = attention_mask.unsqueeze(-1).expand(last_hidden.size()).float()
            summed = torch.sum(last_hidden * mask, dim=1)
            counts = torch.clamp(mask.sum(dim=1), min=1e-9)
            pooled = summed / counts
            return {"logits": self.classifier(pooled)}

    _DEVICE = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    _TOKENIZER = AutoTokenizer.from_pretrained(MODEL_DIR)
    _MODEL = DesklibAIDetectionModel.from_pretrained(MODEL_DIR).to(_DEVICE).eval()


def _score_batch(texts: list[str]) -> list[float]:
    """Return P(AI) for each text in [0,1]."""
    import torch

    _load_model()
    enc = _TOKENIZER(
        texts,
        padding=True,
        truncation=True,
        max_length=MAX_TOKENS,
        return_tensors="pt",
    ).to(_DEVICE)
    with torch.no_grad():
        logits = _MODEL(input_ids=enc["input_ids"], attention_mask=enc["attention_mask"])["logits"]
        probs = torch.sigmoid(logits).squeeze(-1)
    return probs.cpu().tolist()


def analyze(text: str) -> dict:
    """Score every sentence and compute the AI-flagged share of the document.

    ``ai_share`` = fraction of the text (weighted by sentence length) whose
    per-sentence AI probability exceeds SENT_DECISION. This mirrors how Turnitin
    reports "% of the document detected as AI", so it can be compared against
    the professor's <20% criterion (still a different model, so not identical).
    ``mean_prob`` is kept as a secondary signal.
    """
    sentences = split_sentences(text)
    if not sentences:
        return {"ai_share": 0.0, "mean_prob": 0.0, "sentences": []}

    scored: list[dict] = []
    for i in range(0, len(sentences), BATCH_SIZE):
        batch = sentences[i : i + BATCH_SIZE]
        for sent, prob in zip(batch, _score_batch(batch)):
            scored.append({"text": sent, "prob": float(prob)})

    total = sum(len(s["text"]) for s in scored) or 1
    ai_chars = sum(len(s["text"]) for s in scored if s["prob"] >= SENT_DECISION)
    mean_prob = sum(s["prob"] * len(s["text"]) for s in scored) / total
    return {"ai_share": ai_chars / total, "mean_prob": mean_prob, "sentences": scored}


# --------------------------------------------------------------------------- #
# Reporting
# --------------------------------------------------------------------------- #
def build_report(result: dict, threshold: float, top_n: int, passed: bool) -> str:
    share = result["ai_share"]
    status = "✅ PASS" if passed else "❌ FAIL"
    lines = [
        "## 🤖 AI-writing pre-check",
        "",
        f"_Detector: `{MODEL_DIR}` (local, open-source)._",
        "",
        f"**{status}** — share of the document detected as AI: **{share:.1%}** "
        f"(must stay under {threshold:.0%})",
        f"<sub>secondary signal — mean AI confidence: {result['mean_prob']:.1%}</sub>",
        "",
        "> ⚠️ Not Turnitin and not comparable to it. Early-warning proxy only — "
        "treat as a signal, not a verdict.",
        "",
    ]
    suspicious = sorted(
        (s for s in result["sentences"] if len(s["text"]) >= MIN_SENTENCE_CHARS),
        key=lambda s: s["prob"],
        reverse=True,
    )[:top_n]
    if suspicious:
        lines.append(f"### Top {len(suspicious)} most AI-like sentences")
        lines.append("")
        for s in suspicious:
            snippet = textwrap.shorten(s["text"], width=160, placeholder="…")
            lines.append(f"- **{s['prob']:.0%}** — {snippet}")
        lines.append("")
    return "\n".join(lines)


def write_step_summary(report: str) -> None:
    path = os.environ.get("GITHUB_STEP_SUMMARY")
    if path:
        with open(path, "a", encoding="utf-8") as fh:
            fh.write(report + "\n")


def write_report_file(report: str) -> None:
    """Write the report to AI_REPORT_FILE so a PR-comment step can read it."""
    path = os.environ.get("AI_REPORT_FILE")
    if path:
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(report + "\n")


# --------------------------------------------------------------------------- #
# Main
# --------------------------------------------------------------------------- #
def main(argv: list[str]) -> int:
    if len(argv) != 2:
        sys.exit("Usage: python scripts/ai_check.py <path-to.tex>")
    tex_path = argv[1]
    if not os.path.isfile(tex_path):
        sys.exit(f"ERROR: file not found: {tex_path}")

    threshold = float(os.environ.get("AI_THRESHOLD", DEFAULT_THRESHOLD))
    top_n = int(os.environ.get("AI_TOP_N", DEFAULT_TOP_N))

    prose = extract_prose(tex_path)
    if len(prose) < 200:
        sys.exit(
            f"ERROR: extracted only {len(prose)} chars of prose from {tex_path}; "
            "extraction likely failed. Check pandoc output."
        )

    result = analyze(prose)
    passed = result["ai_share"] <= threshold
    report = build_report(result, threshold, top_n, passed)

    print(report)
    write_step_summary(report)
    write_report_file(report)

    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
