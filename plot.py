import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import pandas as pd
import json


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

        # Antteech point with its mean value
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


plot_cutoff_impact("./bench/results/srv2/base.json").savefig(
    "bench/plots/srv2-cutoff-analysis.svg"
)
