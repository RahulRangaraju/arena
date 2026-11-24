#!/usr/bin/env python3
"""Turn benchmark CSV into the README table and a log-scale chart.

    ./build/arena --bench | tee bench/results.csv
    python3 bench/plot.py bench/results.csv          # writes bench/collision.png, prints table

Needs matplotlib:  pip install matplotlib   (or: sudo apt install python3-matplotlib)
"""
import csv
import sys
from pathlib import Path

GRID_COLOR = "#2a78d6"    # categorical slot 1 (blue)
NAIVE_COLOR = "#eb6834"   # categorical slot 2 (orange) - validated pair, CVD-safe
INK, INK_MUTED, GRIDLINE = "#1f1f1e", "#6b6a64", "#e6e5e0"
FRAME_BUDGET_MS = 1000 / 60   # 16.7 ms


def load(path):
    lines = [l for l in Path(path).read_text().splitlines() if l and not l.startswith("#")]
    rows = list(csv.DictReader(lines))
    machine = next((l[2:] for l in Path(path).read_text().splitlines()
                    if l.startswith("# arena benchmark")), "")
    return rows, machine


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "bench/results.csv"
    rows, machine = load(src)
    main_rows = [r for r in rows if r["cell"] == "32"]
    by = {(r["mode"], int(r["enemies"])): r for r in main_rows}
    counts = sorted({int(r["enemies"]) for r in main_rows})

    # ---- Markdown table -------------------------------------------------------------
    print(f"<!-- {machine} -->")
    print("| Enemies | Bullets | Naive collide (ms) | Grid collide (ms) | Speedup |")
    print("|---:|---:|---:|---:|---:|")
    for n in counts:
        g, v = by.get(("grid", n)), by.get(("naive", n))
        if not (g and v):
            continue
        ng, nv = float(g["collide_ms"]), float(v["collide_ms"])
        print(f"| {n:,} | {int(float(g['bullets'])):,} | {nv:.2f} | {ng:.2f} | {nv / ng:.1f}× |")

    cells = sorted((r for r in rows if r["mode"] == "grid" and r["enemies"] == "10000"),
                   key=lambda r: float(r["cell"]))
    if len(cells) > 1:
        print("\n| Cell size (px) | Grid collide at 10,000 enemies (ms) |")
        print("|---:|---:|")
        for r in cells:
            print(f"| {r['cell']} | {float(r['collide_ms']):.2f} |")

    # ---- Chart ----------------------------------------------------------------------
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        import matplotlib.ticker
    except ImportError:
        print("\n(matplotlib not installed - skipping chart)", file=sys.stderr)
        return

    fig, ax = plt.subplots(figsize=(8, 4.8), dpi=150)
    fig.patch.set_facecolor("#fcfcfb")
    ax.set_facecolor("#fcfcfb")

    series = [("naive", "Naive O(B×E)", NAIVE_COLOR, "s"), ("grid", "Spatial grid", GRID_COLOR, "o")]
    for mode, label, color, marker in series:
        xs = [n for n in counts if (mode, n) in by]
        ys = [float(by[(mode, n)]["collide_ms"]) for n in xs]
        ax.plot(xs, ys, color=color, linewidth=2, marker=marker, markersize=7,
                markeredgecolor="#fcfcfb", markeredgewidth=2, label=label, zorder=3)
        ax.annotate(f"{label}  {ys[-1]:.1f} ms", (xs[-1], ys[-1]), xytext=(8, 0),
                    textcoords="offset points", va="center", fontsize=9, color=INK)

    ax.axhline(FRAME_BUDGET_MS, color=INK_MUTED, linewidth=1, linestyle=(0, (4, 3)), zorder=2)
    ax.annotate("60 FPS frame budget (16.7 ms)", (counts[-1], FRAME_BUDGET_MS), xytext=(0, -12),
                textcoords="offset points", ha="right", fontsize=8, color=INK_MUTED)

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xticks(counts)
    ax.set_xticklabels([f"{n:,}" for n in counts])
    ax.minorticks_off()
    ax.yaxis.set_major_formatter(matplotlib.ticker.FuncFormatter(lambda v, _: f"{v:g}"))
    ax.set_xlabel("Enemies (≈13k bullets in flight)", color=INK_MUTED, fontsize=9)
    ax.set_ylabel("Collision pass per update (ms, log)", color=INK_MUTED, fontsize=9)
    ax.set_title("Collision time: naive vs spatial grid", loc="left", color=INK, fontsize=12)
    ax.grid(True, which="major", color=GRIDLINE, linewidth=0.8, zorder=0)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)
    for side in ("left", "bottom"):
        ax.spines[side].set_color(GRIDLINE)
    ax.tick_params(colors=INK_MUTED, labelsize=8)
    ax.legend(frameon=False, fontsize=9, loc="upper left", labelcolor=INK)
    ax.set_xlim(counts[0] * 0.8, counts[-1] * 2.6)   # room for end labels
    if machine:
        fig.text(0.01, 0.01, machine, fontsize=6.5, color=INK_MUTED)

    out = Path(src).with_name("collision.png")
    fig.tight_layout(rect=(0, 0.03, 1, 1))
    fig.savefig(out)
    print(f"\nwrote {out}", file=sys.stderr)


if __name__ == "__main__":
    main()
