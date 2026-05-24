# Compile preview PDF

```powershell
$env:PATH = "$env:LOCALAPPDATA\Programs\MiKTeX\miktex\bin\x64;" + $env:PATH
cd docs/arxiv/paper
pdflatex -interaction=nonstopmode main.tex
bibtex main
pdflatex -interaction=nonstopmode main.tex
pdflatex -interaction=nonstopmode main.tex
```

**Public repo (T1):** ships **`main.tex` + `references.bib` only** — no `main.pdf` until arxiv release tag.

Local output: **`main.pdf`** — upload to arxiv; add to public repo only with tag `t1-arxiv-*` (not `t1-eval-20260522`).

**arXiv:** primary **cs.DS**; optional cross-list cs.DC / cs.LG at metadata step.
