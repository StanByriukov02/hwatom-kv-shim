# Compile preview PDF

```powershell
$env:PATH = "$env:LOCALAPPDATA\Programs\MiKTeX\miktex\bin\x64;" + $env:PATH
cd docs/arxiv/paper
pdflatex -interaction=nonstopmode main.tex
bibtex main
pdflatex -interaction=nonstopmode main.tex
pdflatex -interaction=nonstopmode main.tex
```

Output: **`main.pdf`** (preview; GitHub URL `[TBD]` until T1 push).

**arXiv:** primary **cs.DS**; optional cross-list cs.DC / cs.LG at metadata step.
