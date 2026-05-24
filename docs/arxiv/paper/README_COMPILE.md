# Compile preview PDF

```powershell
$env:PATH = "$env:LOCALAPPDATA\Programs\MiKTeX\miktex\bin\x64;" + $env:PATH
cd docs/arxiv/paper
pdflatex -interaction=nonstopmode main.tex
bibtex main
pdflatex -interaction=nonstopmode main.tex
pdflatex -interaction=nonstopmode main.tex
```

Output: **`main.pdf`** — artifact repo: https://github.com/StanByriukov02/hwatom-gate-12s tag `t1-eval-20260522`.

**arXiv:** primary **cs.DS**; optional cross-list cs.DC / cs.LG at metadata step.
