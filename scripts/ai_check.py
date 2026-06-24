#!/usr/bin/env python3
"""AI-writing pre-check for the LaTeX paper.

Scores the body prose of a .tex file with a free, local AI-text detector
(desklib/ai-text-detector) and prints a Markdown report. Exits non-zero when the
average AI score exceeds AI_THRESHOLD (default 0.18, below the professor's 20%),
so it can gate a CI check. This is NOT Turnitin and does not reproduce its
numbers — it is an aligned proxy that mirrors Turnitin's method: ~250-word
sliding windows scored and averaged, non-prose (figures, captions, headings,
bibliography, math) stripped. Tables and lists are kept in on purpose, a
conservative (stricter-than-Turnitin) choice.

Usage:
    python scripts/ai_check.py docs/paper/main.tex

Environment:
    AI_THRESHOLD     Max average AI score before failing. Default 0.18.
    AI_WINDOW_WORDS  Sliding-window size in words. Default 250.
    AI_TOP_N         Max flagged passages to list. Default 50.
    AI_MODEL         HF model id. Default desklib/ai-text-detector-v1.01.
    AI_REPORT_FILE   Write the Markdown report to this path.
    AI_ANNOTATE_OUT  Write a copy of the .tex with flagged paragraphs highlighted.
"""

from __future__ import annotations

import os
import re
import subprocess
import sys
import textwrap

MODEL_DIR = os.environ.get("AI_MODEL", "desklib/ai-text-detector-v1.01")
MAX_TOKENS = 768
BATCH_SIZE = 4
WINDOW_WORDS = int(os.environ.get("AI_WINDOW_WORDS", "250"))
MIN_PROSE_WORDS = 50
DISPLAY_CUTOFF = 0.50
DEFAULT_THRESHOLD = 0.18
DEFAULT_TOP_N = 50


def _tabular_to_prose(tex: str) -> str:
    """Replace each tabular with just its cell text, dropping the header row and
    rules/column layout. Keeps the real written content of tables without the
    ASCII-table noise (headers, dashes) that pandoc would otherwise produce."""
    colspec = r"(?:\[[^\]]*\]\s*)?\{(?:[^{}]|\{[^{}]*\})*\}"

    def repl(match: "re.Match") -> str:
        inner = match.group(1)
        inner = re.sub(r"\\(top|mid|bottom)rule|\\hline|\\cmidrule[^\n]*", " ", inner)
        rows = re.split(r"\\\\", inner)
        out = []
        for r, row in enumerate(rows):
            if r == 0:  # header row
                continue
            cells = []
            for cell in row.split("&"):
                cell = re.sub(r"\\textbf\{([^}]*)\}", r"\1", cell)
                cell = re.sub(r"\\cite\w*\s*\{[^}]*\}", " ", cell)
                cell = re.sub(r"\\[a-zA-Z]+\*?(\[[^\]]*\])?", " ", cell)
                cell = re.sub(r"[{}$]", " ", cell)
                cell = re.sub(r"\s+", " ", cell).strip()
                if cell:
                    cells.append(cell.rstrip("."))
            if cells:
                out.append(". ".join(cells))
        return "\n\n" + "\n\n".join(out) + "\n\n"

    return re.sub(rf"\\begin\{{tabular\}}\s*{colspec}(.*?)\\end\{{tabular\}}",
                  repl, tex, flags=re.DOTALL)


def _filter_body(tex: str) -> str:
    """Strip non-prose so the analyzed text mirrors Turnitin's qualifying text:
    title/author metadata, figures, captions, math, headings, bibliography and
    citation commands. Table cell text is kept (conservative choice) but the
    table layout/headers are dropped."""
    m = re.search(r"\\begin\{document\}(.*?)\\end\{document\}", tex, re.DOTALL)
    tex = m.group(1) if m else tex
    tex = _tabular_to_prose(tex)
    for env in ("figure", "tikzpicture", "thebibliography",
                "equation", "align", "displaymath", "IEEEkeywords"):
        tex = re.sub(rf"\\begin\{{{env}\*?\}}.*?\\end\{{{env}\*?\}}",
                     "\n\n", tex, flags=re.DOTALL)
    for cmd in ("title", "author"):
        tex = re.sub(rf"\\{cmd}\s*\{{(?:[^{{}}]|\{{[^{{}}]*\}})*\}}",
                     "\n\n", tex, flags=re.DOTALL)
    tex = re.sub(r"\\maketitle", " ", tex)
    tex = re.sub(r"\\(section|subsection|subsubsection|paragraph)\*?\s*\{[^}]*\}",
                 "\n\n", tex)
    tex = re.sub(r"\\caption\s*\{(?:[^{}]|\{[^{}]*\})*\}", " ", tex, flags=re.DOTALL)
    tex = re.sub(r"\\(cite\w*|ref|label|bibliographystyle|bibliography|input|include)\s*\{[^}]*\}",
                 " ", tex)
    tex = re.sub(r"\$\$.*?\$\$", " ", tex, flags=re.DOTALL)
    tex = re.sub(r"\$[^$]*\$", " ", tex)
    return tex


def extract_prose(tex_path: str) -> str:
    """Return body prose, via pandoc, falling back to a regex stripper."""
    with open(tex_path, "r", encoding="utf-8") as fh:
        filtered = _filter_body(fh.read())
    try:
        result = subprocess.run(
            ["pandoc", "-f", "latex", "-t", "plain", "--quiet"],
            input=filtered,
            capture_output=True,
            text=True,
            timeout=120,
        )
        if result.returncode == 0 and result.stdout.strip():
            return _normalize(result.stdout)
        sys.stderr.write(f"pandoc failed; using regex fallback.\n{result.stderr}\n")
    except FileNotFoundError:
        sys.stderr.write("pandoc not found; using regex fallback.\n")
    except subprocess.TimeoutExpired:
        sys.stderr.write("pandoc timed out; using regex fallback.\n")
    return _strip_latex(filtered)


def _strip_latex(tex: str) -> str:
    """Regex LaTeX->prose fallback used only when pandoc is unavailable."""
    tex = re.sub(r"%.*", "", tex)
    tex = re.sub(r"\$[^$]*\$", " ", tex)
    tex = re.sub(r"\\[`'^\"~=.]\{?([a-zA-Z])\}?", r"\1", tex)   # accents -> base letter
    tex = tex.replace("\\\\", "\n")
    tex = re.sub(r"\\[a-zA-Z]+\*?(\[[^\]]*\])?", " ", tex)
    tex = re.sub(r"[{}]", " ", tex)
    return _normalize(tex)


def _normalize(text: str) -> str:
    text = re.sub(r"[ \t]+", " ", text)
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip()


def split_sentences(text: str) -> list[str]:
    """Split prose into sentences on terminal punctuation."""
    flat = re.sub(r"\s+", " ", text).strip()
    parts = re.split(r"(?<=[.!?])\s+(?=[A-Z(])", flat)
    return [p.strip() for p in parts if len(p.strip()) >= 3]


def build_windows(sentences: list[str], target_words: int = WINDOW_WORDS) -> list[str]:
    """Overlapping ~target_words windows with a one-sentence stride, mirroring
    Turnitin's segment windowing."""
    windows: list[str] = []
    n = len(sentences)
    for i in range(n):
        words, j, buf = 0, i, []
        while j < n and words < target_words:
            buf.append(sentences[j])
            words += len(sentences[j].split())
            j += 1
        windows.append(" ".join(buf))
        if j >= n:
            break
    return windows


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
    """Return P(AI) in [0,1] for each text."""
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
    """Score overlapping windows and average their AI scores (ai_pct), the way
    Turnitin averages its segment scores."""
    sentences = split_sentences(text)
    words = sum(len(s.split()) for s in sentences)
    if words < MIN_PROSE_WORDS:
        return {"ai_pct": 0.0, "words": words, "windows": []}

    windows = build_windows(sentences)
    scored: list[dict] = []
    for i in range(0, len(windows), BATCH_SIZE):
        batch = windows[i : i + BATCH_SIZE]
        for win, prob in zip(batch, _score_batch(batch)):
            scored.append({"text": win, "prob": float(prob)})

    ai_pct = sum(s["prob"] for s in scored) / (len(scored) or 1)
    return {"ai_pct": ai_pct, "words": words, "windows": scored}


def build_report(result: dict, threshold: float, top_n: int, passed: bool) -> str:
    pct = result["ai_pct"]
    status = "✅ PASS" if passed else "❌ FAIL"
    lines = [
        "## 🤖 AI-writing check",
        "",
        f"_Detector: `{MODEL_DIR}` (local). Turnitin-style: ~{WINDOW_WORDS}-word "
        f"sliding windows, scores averaged. {result['words']} prose words "
        "(tables and lists kept in, unlike Turnitin)._",
        "",
        f"**{status}** — **{pct:.1%}** of the prose reads as AI-generated "
        f"(must stay under {threshold:.0%}).",
        "",
        "> ⚠️ Mirrors Turnitin's method but uses a different model — NOT a "
        "Turnitin score, a conservative early-warning signal.",
        "",
    ]
    # Overlapping windows repeat content, so de-duplicate by leading text.
    suspicious = sorted(
        (s for s in result["windows"] if s["prob"] >= DISPLAY_CUTOFF),
        key=lambda s: s["prob"],
        reverse=True,
    )
    seen: set[str] = set()
    uniq: list[dict] = []
    for s in suspicious:
        key = s["text"][:60]
        if key in seen:
            continue
        seen.add(key)
        uniq.append(s)
        if len(uniq) >= top_n:
            break
    if uniq:
        lines.append(f"### Flagged passages to revise ({len(uniq)})")
        lines.append("")
        for s in uniq:
            snippet = textwrap.shorten(s["text"], width=260, placeholder="…")
            lines.append(f"- **{s['prob']:.0%}** — {snippet}")
        lines.append("")
    return "\n".join(lines)


def write_step_summary(report: str) -> None:
    path = os.environ.get("GITHUB_STEP_SUMMARY")
    if path:
        with open(path, "a", encoding="utf-8") as fh:
            fh.write(report + "\n")


def write_report_file(report: str) -> None:
    path = os.environ.get("AI_REPORT_FILE")
    if path:
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(report + "\n")


# Annotated .tex: highlights AI-flagged source paragraphs.
ENV_PROTECT = ("table", "figure", "tikzpicture", "thebibliography", "itemize",
               "enumerate", "equation", "align", "displaymath", "abstract",
               "IEEEkeywords")

HIGHLIGHT_PREAMBLE = r"""
% --- AI-writing annotation (auto-generated; do not edit by hand) ---
\usepackage{xcolor}
\usepackage[framemethod=default]{mdframed}
\definecolor{aihlbg}{rgb}{1.0,0.92,0.55}
\newmdenv[backgroundcolor=aihlbg,hidealllines=true,skipabove=3pt,skipbelow=3pt,
innertopmargin=2pt,innerbottommargin=2pt,innerleftmargin=3pt,innerrightmargin=3pt]{aibox}
\newcommand{\aitag}[1]{{\footnotesize\sffamily\bfseries\textcolor{orange!75!black}{[AI~#1\%]}~}}
% --- end AI-writing annotation ---
"""


def _block_prose(block: str) -> str:
    b = re.sub(r"\\(cite\w*|ref|label)\s*\{[^}]*\}", " ", block)
    b = re.sub(r"\$[^$]*\$", " ", b)
    b = re.sub(r"\\[a-zA-Z]+\*?(\[[^\]]*\])?", " ", b)
    b = re.sub(r"[{}]", " ", b)
    return re.sub(r"\s+", " ", b).strip()


def _is_prose_block(block: str) -> bool:
    """True for real body paragraphs (skip comments, headings, commands)."""
    noncomment = [ln for ln in block.splitlines()
                  if ln.strip() and not ln.strip().startswith("%")]
    if not noncomment:
        return False
    if re.match(r"\\(section|subsection|subsubsection|paragraph|title|author|"
                r"maketitle|bibliography|bibliographystyle|begin|end|item|caption|"
                r"label|IEEE)", noncomment[0].lstrip()):
        return False
    return len(_block_prose(block).split()) >= 12


def annotate_tex(tex_path: str, cutoff: float = DISPLAY_CUTOFF) -> str:
    """Return a copy of the .tex with AI-flagged paragraphs wrapped in a tagged
    highlight box. Environments (tables, figures, lists) are left untouched."""
    with open(tex_path, "r", encoding="utf-8") as fh:
        raw = fh.read()
    m = re.search(r"\\begin\{document\}(.*)\\end\{document\}", raw, re.DOTALL)
    if not m:
        return raw
    head, body, tail = raw[: m.start()], m.group(1), raw[m.end():]

    placeholders: dict[str, str] = {}

    def _protect(match: "re.Match") -> str:
        key = f"@@AIPROT{len(placeholders)}@@"
        placeholders[key] = match.group(0)
        return f"\n\n{key}\n\n"

    env_re = re.compile(
        r"\\begin\{(" + "|".join(ENV_PROTECT) + r")\*?\}.*?\\end\{\1\*?\}", re.DOTALL)
    body = env_re.sub(_protect, body)

    blocks = re.split(r"(\n\s*\n)", body)
    idx = [i for i, b in enumerate(blocks) if _is_prose_block(b)]
    texts = [_block_prose(blocks[i]) for i in idx]
    scores: dict[int, float] = {}
    for k in range(0, len(texts), BATCH_SIZE):
        for j, prob in zip(idx[k : k + BATCH_SIZE], _score_batch(texts[k : k + BATCH_SIZE])):
            scores[j] = prob
    for i in idx:
        if scores[i] >= cutoff:
            blocks[i] = (f"\\begin{{aibox}}\\aitag{{{round(scores[i] * 100)}}}"
                         f"{blocks[i].strip()}\\end{{aibox}}")
    body = "".join(blocks)
    for key, val in placeholders.items():
        body = body.replace(key, val)
    return head + HIGHLIGHT_PREAMBLE + "\\begin{document}" + body + "\\end{document}" + tail


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
        sys.exit(f"ERROR: extracted only {len(prose)} chars of prose; extraction failed.")

    result = analyze(prose)
    passed = result["ai_pct"] <= threshold
    report = build_report(result, threshold, top_n, passed)

    print(report)
    write_step_summary(report)
    write_report_file(report)

    annotate_out = os.environ.get("AI_ANNOTATE_OUT")
    if annotate_out:
        with open(annotate_out, "w", encoding="utf-8") as fh:
            fh.write(annotate_tex(tex_path))

    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
