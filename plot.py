import json
from pathlib import Path
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
    plt.ylabel("Mean time (s)")
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


def plot_baselines_cmp(
    machine_id: str,
    include_fresh: bool,
    xticks=None,
    yticks=None,
    cities_filter_min=0,
    single_cutoff=None,
):
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

    plt.figure(figsize=(14, 7))
    annot_shift = 0
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
            if single_cutoff is not None and entry["cutoff"] is not single_cutoff:
                continue
            if entry["cities"] < cities_filter_min:
                continue
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
            # Annotate each point with its mean value
            for x, y in zip(cities, means):
                plt.annotate(
                    f"{y:.2f}",
                    (x, y),
                    textcoords="offset points",
                    xytext=(0, 6 + annot_shift),
                    ha="center",
                    fontsize=9,
                    color=color,
                )

            annot_shift += 9
            if annot_shift > 18:
                annot_shift = 0

    plt.xlabel("Number of cities")
    plt.ylabel("Mean time (s)")

    plt.title("Versions comparison")
    ax = plt.gca()
    # ax.set_yscale("log") # yes or not
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


import re
from typing import List, Dict, Optional


def parse_benchmark_output(path_str: str) -> List[Dict[str, object]]:
    """
    Parse benchmark output and extract successful benchmark results.

    Returns a list of dictionaries with keys:
    - cities (int)
    - threads (int)
    - cutoff (int)
    - mean (float)
    - aborts (int)
    - empties (int)
    """
    # Read the benchmark output file
    path = Path(path_str)
    text = path.read_text(encoding="utf-8", errors="ignore")

    benchmark_re = re.compile(r"Benchmark\s+\d+:\s+.*\s+(\d+)\s+(\d+)\s+(\d+)")
    parallel_re = re.compile(r"parallel:.*\bt:([0-9.]+)")
    counts_re = re.compile(
        r"with aborts count\s*=\s*(\d+)\s*and empties count\s*=\s*(\d+)"
    )

    results = []
    current: Optional[Dict[str, object]] = None
    waiting_for_parallel = False

    lines = text.splitlines()

    for line in lines:
        # New benchmark detected
        m = benchmark_re.search(line)
        if m:
            # If we were waiting for a parallel result, discard it (failed run)
            current = {
                "cities": int(m.group(1)),
                "threads": int(m.group(2)),
                "cutoff": int(m.group(3)),
            }
            waiting_for_parallel = True
            continue

        if not waiting_for_parallel or current is None:
            continue

        # Look for parallel result
        m = parallel_re.search(line)
        if m:
            current["mean"] = float(m.group(1))
            continue

        # Look for aborts / empties line
        m = counts_re.search(line)
        if m and "mean" in current:
            current["aborts"] = int(m.group(1))
            current["empties"] = int(m.group(2))

            # Successful benchmark → store it
            results.append(current)

            # Reset state
            current = None
            waiting_for_parallel = False

    return results


def deduplicate_results(results_lists):
    """
    Deduplicate benchmark results by (cities, threads, cutoff).

    If the same key appears multiple times, the *last* occurrence wins.
    """

    dedup = {}

    for results in results_lists:
        for r in results:
            key = (r["cities"], r["threads"], r["cutoff"])
            dedup[key] = r  # overwrite → last one wins

    # Optional: return sorted list
    return sorted(
        dedup.values(), key=lambda r: (r["cities"], r["threads"], r["cutoff"])
    )


def plot_abort_empty_ratio(results):
    """
    X axis: cities
    Y axis: (aborts + empties) / threads
    """

    # Group points by thread count (for colors / legend)
    by_threads = defaultdict(list)

    for r in results:
        ratio = (r["aborts"] + r["empties"]) / r["threads"]
        by_threads[r["threads"]].append((r["cities"], ratio))

    plt.figure(figsize=(10, 6))

    for threads, points in sorted(by_threads.items()):
        xs = [p[0] for p in points]
        ys = [p[1] for p in points]

        plt.scatter(xs, ys, label=f"{threads} threads")

        # Label each point with the ratio
        for x, y in points:
            plt.annotate(
                f"{y:.1f}",
                (x, y),
                textcoords="offset points",
                xytext=(5, 5),
                fontsize=8,
            )

    plt.xlabel("Number of cities")
    plt.ylabel("(aborts + empties) / threads")
    plt.title("Abort + Empty ratio per thread vs cities")
    plt.legend(title="Threads")
    plt.grid(True, linestyle="--", alpha=0.4)

    plt.tight_layout()
    return plt


from collections import defaultdict


def plot_aborts_and_empties_vs_threads(results):
    """
    X axis: threads
    Y axis: aborts / empties
    One line per city for aborts
    One line per city for empties
    """

    # Structure: city -> {threads -> value}
    aborts = defaultdict(dict)
    empties = defaultdict(dict)

    for r in results:
        city = r["cities"]
        threads = r["threads"]
        aborts[city][threads] = r["aborts"]
        empties[city][threads] = r["empties"]

    cities = sorted(aborts.keys())

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8), sharex=True)

    # --- Aborts subplot ---
    for city in cities:
        xs = sorted(aborts[city].keys())
        ys = [aborts[city][t] for t in xs]
        ax1.plot(xs, ys, marker="o", label=f"{city} cities")

    ax1.set_ylabel("Aborts count")
    ax1.set_title("Aborts vs Threads")
    ax1.grid(True, linestyle="--", alpha=0.4)
    ax1.legend()

    # --- Empties subplot ---
    for city in cities:
        xs = sorted(empties[city].keys())
        ys = [empties[city][t] for t in xs]
        ax2.plot(xs, ys, marker="o", label=f"{city} cities")

    ax2.set_xlabel("Threads")
    ax2.set_ylabel("Empties count")
    ax2.set_title("Empties vs Threads")
    ax2.grid(True, linestyle="--", alpha=0.4)
    ax2.legend()

    plt.tight_layout()
    return plt


def load_start_times(path: Path) -> dict[int, float]:
    """
    Load start.json: cities -> mean time
    """
    data = json.loads(path.read_text())
    return {int(k): float(v) for k, v in data.items()}


from collections import defaultdict


def compute_speedup(results, start_times):
    """
    Returns:
      city -> {threads -> speedup}
    """
    by_city = defaultdict(dict)

    for r in results:
        city = r["cities"]
        threads = r["threads"]

        if city not in start_times:
            continue  # ignore cities not in start.json

        speedup = start_times[city] / r["mean"]
        by_city[city][threads] = speedup

    return by_city


def plot_speedup(speedup_by_city, version_name):
    plt.figure(figsize=(10, 6))

    for city in sorted(speedup_by_city.keys()):
        xs = sorted(speedup_by_city[city].keys())
        ys = [speedup_by_city[city][t] for t in xs]

        plt.plot(xs, ys, marker="o", label=f"{city} cities")

        # Optional point labels
        for x, y in zip(xs, ys):
            plt.annotate(
                f"{y:.2f}",
                (x, y),
                textcoords="offset points",
                xytext=(5, 5),
                fontsize=8,
            )

    plt.xlabel("Threads")
    plt.ylabel("Speedup (start / mean)")
    plt.title(f"Speedup vs Threads — {version_name}")
    plt.grid(True, linestyle="--", alpha=0.4)
    plt.legend(title="Cities")
    plt.tight_layout()
    return plt


def gen_speedup_plot():
    MACHINE_ID = "srv2"
    START_JSON = Path("./bench/results/srv2/start.json")

    SELECTED_VERSIONS = {"base", "first-wsd", "final"}

    start_times = load_start_times(START_JSON)

    for name, git_hash, desc in load_versions():
        if name not in SELECTED_VERSIONS:
            continue

        path = results_file(MACHINE_ID, name)
        if not path.exists():
            continue

        results = load_results(path)
        speedup = compute_speedup(results, start_times)

        file = f"bench/plots/srv2-speedup-{name}.svg"
        plot_speedup(speedup, name).savefig(file)
        print(f"Generated {file}")


# ------

gen_speedup_plot()

print("Plots generation")

print("Printing txt results parsed")
# Parse results
megaresults = [
    parse_benchmark_output("./bench/results/srv2/txt/hyperfinebythreadsafterfix1.txt"),
    parse_benchmark_output("./bench/results/srv2/txt/hyperfinebythreadsafterfix2.txt"),
    parse_benchmark_output("./bench/results/srv2/txt/hyperfinebythreadsafterfix3.txt"),
]
results = deduplicate_results(megaresults)

file = "bench/plots/srv2-abort-empty-ratio.svg"
plot_abort_empty_ratio(results).savefig(file)
print(f"Generated {file}")

file = "bench/plots/srv2-abort-and-empties-vs-threads.svg"
plot_aborts_and_empties_vs_threads(results).savefig(file)
print(f"Generated {file}")


# Print results
for r in results:
    print(
        f"cities={r['cities']}, "
        f"threads={r['threads']}, "
        f"cutoff={r['cutoff']}, "
        f"mean={r['mean']}, "
        f"aborts={r['aborts']}, "
        f"empties={r['empties']}"
    )

exit(2)

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
# xticks = [5, 6, 7, 8, 10, 12, 13, 14, 15, 16, 17, 18]
xticks = [13, 14, 15, 16]
yticks = [0.05, 0.1, 0.5, 1, 2, 5, 10, 30, 50, 100]
plot_baselines_cmp("srv2", True, xticks, yticks, 13).savefig(file)
print(f"Generated {file}")

file = "bench/plots/sam-baseline-cmp.svg"
xticks = [13, 14, 15, 16]
yticks = [0.05, 0.1, 0.5, 1, 2, 3]
plot_baselines_cmp("sam", True, xticks, yticks, 13).savefig(file)
print(f"Generated {file}")

file = "bench/plots/olivia-baseline-cmp.svg"
xticks = [10, 12, 13, 14, 15]
yticks = [0.05, 0.1, 0.2, 0.3, 0.4, 0.5]
plot_baselines_cmp("olivia", False, xticks, yticks, 10, 8).savefig(file)
print(f"Generated {file}")
