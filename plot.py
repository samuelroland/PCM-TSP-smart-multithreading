import json
import os
from bench import load_machine_id, load_versions, results_file

from collections import defaultdict
from itertools import cycle
import matplotlib as mpl
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import pandas as pd

# Make plots images deterministic !
os.environ["SOURCE_DATE_EPOCH"] = "1765722353"  # hardcode time of now
mpl.rcParams["svg.hashsalt"] = "fixed"  # fix the hashsalt for svg id attributes


def load_results(json_path):
    with open(json_path, "r") as f:
        return json.load(f)


def plot_cutoff_impact(json_path):
    data = load_results(json_path)
    df = pd.DataFrame(data)

    df = df.sort_values(by=["cities", "cutoff"])

    plt.figure(figsize=(14, 7))

    shift = 0
    for cities, group in df.groupby("cities"):
        plt.plot(group["cutoff"], group["mean"], marker="o", label=f"{cities} cities")

        # Annotate point with its mean value
        for x, y in zip(group["cutoff"], group["mean"]):
            plt.annotate(
                f"{y:.2f}",
                (x, y),
                textcoords="offset points",
                xytext=(0, 6 + shift),
                ha="center",
                fontsize=11,
            )
        shift = shift + 5
        if shift > 8:
            shift = 0

    plt.xlabel("Cutoff")
    plt.ylabel("Mean time (seconds)")
    # plt.title("Cutoff impact depending on cities numbers, on starting code")

    ax = plt.gca()
    ax.set_yscale("log")

    yticks = [0.2, 0.4, 0.6, 0.8, 1, 2, 5, 10, 30, 50, 70]

    ax.set_yticks(yticks)
    ax.set_yticklabels([str(y) for y in yticks])

    # Grid and legend
    plt.grid(True, linestyle="--", alpha=0.5)
    plt.legend(title="Legends")

    plt.tight_layout()
    return plt


def plot_threads_impact(json_path, xticks=None, yticks=None):
    data = load_results(json_path)
    df = pd.DataFrame(data)

    # Sort for proper line drawing
    df = df.sort_values(by=["cities", "threads"])

    plt.figure(figsize=(14, 7))

    shift = 0
    for cities, group in df.groupby("cities"):
        plt.plot(
            group["threads"],
            group["mean"],
            marker="o",
            label=f"{cities} cities",
        )

        # Annotate each point with its mean value
        for x, y in zip(group["threads"], group["mean"]):
            plt.annotate(
                f"{y:.2f}",
                (x, y),
                textcoords="offset points",
                xytext=(0, 6 + shift),
                ha="center",
                fontsize=11,
            )

        shift += 13
        if shift > 32:
            shift = 3

    plt.xlabel("Threads")
    plt.ylabel("Mean time (seconds)")
    # plt.title("Threads impact depending on cities numbers")

    # Log Y axis with explicit ticks
    ax = plt.gca()
    ax.set_xscale("log")
    ax.set_yscale("log")

    if xticks is not None:
        ax.set_xticks(xticks)
        ax.set_xticklabels([str(x) for x in xticks])
    if yticks is not None:
        ax.set_yticks(yticks)
        ax.set_yticklabels([str(y) for y in yticks])

    plt.grid(True, linestyle="--", alpha=0.5)
    plt.legend(title="Legends")

    plt.tight_layout()
    return plt


def plot_baselines_cmp(machine_id: str, include_fresh: bool, xticks=None, yticks=None):
    """
    Plot benchmark comparison for all baselines defined in versions file.

    X-axis: number of cities
    Y-axis: mean time
    One line per unique thread count
    Same color per baseline, different line styles per thread count

    Parameters
    ----------
    include_fresh : bool
        Whether to include bench/results/sam/fresh.json as an extra baseline
    """

    # Load baselines from versions file
    baselines = [name for name, _, _ in load_versions()]

    if include_fresh:
        baselines.append("fresh")

    # Prepare plotting helpers
    baseline_colors = {}
    color_cycle = cycle(plt.rcParams["axes.prop_cycle"].by_key()["color"])

    line_styles = ["-", "--", ":", "-.", (0, (3, 1, 1, 1))]
    style_cycle = cycle(line_styles)

    plt.figure(figsize=(10, 6))

    for baseline in baselines:
        path = results_file(machine_id, baseline)
        if not path.exists():
            continue

        # Assign a color per baseline
        if baseline == "fresh":
            baseline_colors[baseline] = "blue"
        else:
            baseline_colors.setdefault(baseline, next(color_cycle))

        color = baseline_colors[baseline]

        with open(path) as f:
            data = json.load(f)

        # Group by threads
        by_threads = defaultdict(list)
        for entry in data:
            by_threads[entry["threads"]].append(entry)

        # Assign a line style per thread value
        thread_styles = {t: next(style_cycle) for t in sorted(by_threads)}

        for threads, entries in by_threads.items():
            # Sort by cities for clean lines
            entries = sorted(entries, key=lambda e: e["cities"])

            cities = [e["cities"] for e in entries]
            means = [e["mean"] for e in entries]

            label = f"{baseline} | {threads} threads"

            plt.plot(
                cities,
                means,
                linestyle=thread_styles[threads],
                color=color,
                marker="o",
                label=label,
            )

    plt.xlabel("Number of cities")
    plt.ylabel("Time (mean)")

    plt.title("Baseline performance comparison")
    ax = plt.gca()
    ax.set_xscale("log")
    ax.set_yscale("log")
    if xticks is not None:
        ax.set_xticks(xticks)
        ax.set_xticklabels([str(x) for x in xticks])
    if yticks is not None:
        ax.set_yticks(yticks)
        ax.set_yticklabels([str(y) for y in yticks])

    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    return plt


# ------

print("Plots generation")
file = "bench/plots/srv2-cutoff-analysis.svg"
plot_cutoff_impact("./bench/results/srv2/base-cutoff-analysis.json").savefig(file)
print(f"Generated {file}")

xticks = [10, 30, 50, 100, 150, 200, 256, 300, 500, 1000]
yticks = [0.05, 0.1, 0.2, 0.4, 0.6, 1, 2, 5, 10, 30, 50, 70]

file = "bench/plots/srv2-threads-analysis-cutoff-zero.svg"
plot_threads_impact(
    "./bench/results/srv2/srv2-threads-analysis-cutoff-zero.json", xticks, yticks
).savefig(file)

print(f"Generated {file}")

file = "bench/plots/srv2-threads-analysis-cutoff-optimal.svg"
plot_threads_impact(
    "./bench/results/srv2/srv2-threads-analysis-cutoff-optimal.json", xticks, yticks
).savefig(file)
print(f"Generated {file}")


yticks = [0.05, 0.1, 0.2, 0.4, 1, 2, 3, 6]
xticks = [
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    12,
    15,
    20,
    25,
    30,
    50,
    75,
    100,
    200,
]
file = "bench/plots/sam-threads-analysis.svg"
plot_threads_impact(
    "./bench/results/sam/sam-threads-analysis.json", xticks, yticks
).savefig(file)
print(f"Generated {file}")

file = "bench/plots/srv2-baseline-cmp.svg"
xticks = [5, 6, 7, 8, 10, 12, 13, 14, 15, 16, 17, 18]
yticks = [0.05, 0.1, 0.5, 1, 2, 5, 10, 30, 50, 100]
plot_baselines_cmp("srv2", True, xticks, yticks).savefig(file)
print(f"Generated {file}")

file = "bench/plots/sam-baseline-cmp.svg"
xticks = [5, 6, 7, 8, 10, 12, 13, 14, 15, 16, 17, 18]
yticks = [0.05, 0.1, 0.5, 1, 2, 5, 10, 30, 50, 100]
plot_baselines_cmp("sam", True, xticks, yticks).savefig(file)
print(f"Generated {file}")
