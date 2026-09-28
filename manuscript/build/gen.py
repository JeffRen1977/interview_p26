"""Render the annotated TOC and chapter stubs from chapters.py.

Usage: python3 manuscript/build/gen.py            # write TOC; create missing stubs
       python3 manuscript/build/gen.py --check    # print totals only

Existing chapter files are never overwritten.
"""
import sys
from pathlib import Path

from chapters import APPENDICES, CHAPTERS, PARTS, SAMPLE_CHAPTERS, SUBTITLE, TITLE

ROOT = Path(__file__).resolve().parent.parent
READINESS = {
    "drafted": "Drafted",
    "high": "High — restructure existing material",
    "medium": "Medium — substantial notes, needs narrative and gaps filled",
    "low": "Low — thin notes, mostly new writing",
    "new": "New writing",
}


def totals():
    words = sum(c[4] for c in CHAPTERS) + sum(a[4] for a in APPENDICES)
    figs = sum(c[5] for c in CHAPTERS)
    return words, figs


def render_toc():
    words, figs = totals()
    out = [
        f"# Annotated Table of Contents — *{TITLE}*",
        "",
        f"*{SUBTITLE}*",
        "",
        f"{len(PARTS)} parts · {len(CHAPTERS)} chapters · {len(APPENDICES)} appendices · "
        f"~{words:,} words (~{round(words / 300, -1):.0f} printed pages) · ~{figs} original figures",
        "",
        "Sample chapters are marked ★.",
        "",
        "Front matter: a preface on who the book is for and how to use it by role archetype, and a note on the "
        "companion code repository.",
        "",
    ]
    for pnum, pname, pdesc in PARTS:
        out += [f"## Part {pnum} — {pname}", "", f"*{pdesc}*", ""]
        for num, part, _slug, title, w, f, abstract, qs, code, _src, _ready in CHAPTERS:
            if part != pnum:
                continue
            star = " ★" if num in SAMPLE_CHAPTERS else ""
            out += [f"### Chapter {num}. {title}{star}", "", abstract, ""]
            out.append("Questions the chapter prepares the reader to answer: " + " · ".join(f"*{q}*" for q in qs))
            out.append("")
            meta = f"~{w:,} words · {f} figures"
            if code:
                meta += f" · Code: {code}"
            out += [meta, ""]
    out += ["## Appendices", ""]
    for letter, _slug, title, desc, w in APPENDICES:
        out += [f"**Appendix {letter}. {title}** — {desc} (~{w:,} words)", ""]
    return "\n".join(out)


def render_stub(ch):
    num, part, _slug, title, w, f, abstract, qs, code, src, ready = ch
    lines = [
        f"# Chapter {num}. {title}",
        "",
        "<!-- STATUS: not started. Structure and sources come from build/chapters.py. -->",
        f"<!-- Part {part} · target ~{w:,} words · {f} figures · readiness: {READINESS[ready]} -->",
        "<!-- Private source notes (write fresh; never paste): " + ("; ".join(src) if src else "none") + " -->",
        "",
        f"> {abstract}",
        "",
        "## Why interviewers ask this",
        "",
        "## What the loops actually ask",
        "",
        *[f"- {q}" for q in qs],
        "",
        "## The mental model",
        "",
        "## Worked problems",
        "",
        "## Code you can run",
        "",
        f"<!-- {code or 'none planned'} -->",
        "",
        "## In the room",
        "",
        "## Failure modes",
        "",
        "## Exercises",
        "",
    ]
    return "\n".join(lines)


def main():
    words, figs = totals()
    if "--check" in sys.argv:
        print(f"{len(CHAPTERS)} chapters, ~{words:,} words, ~{figs} figures")
        return
    (ROOT / "proposal" / "04-annotated-toc.md").write_text(render_toc() + "\n", encoding="utf-8")
    created = 0
    for ch in CHAPTERS:
        path = ROOT / "chapters" / f"{ch[0]:02d}-{ch[2]}.md"
        if not path.exists():
            path.write_text(render_stub(ch), encoding="utf-8")
            created += 1
    for letter, slug, title, desc, _w in APPENDICES:
        path = ROOT / "chapters" / f"appendix-{letter.lower()}-{slug}.md"
        if not path.exists():
            path.write_text(f"# Appendix {letter}. {title}\n\n<!-- STATUS: not started -->\n\n> {desc}\n", encoding="utf-8")
            created += 1
    print(f"TOC written; {created} stubs created; ~{words:,} words, ~{figs} figures")


if __name__ == "__main__":
    main()
