#!/usr/bin/env python3
# NOTE: this is vibe coded with ChatGPT, and adapted for small fixes.
import argparse
import itertools
import json
import re
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path
from termcolor import colored

# ------------------------
# Constants & Paths
# ------------------------

BENCH_DIR = Path("bench")
CONFIG_DIR = BENCH_DIR / "configs"
RESULTS_DIR = BENCH_DIR / "results"
BIN_DIR = BENCH_DIR / "bin"
TMP_DIR = BENCH_DIR / "tmp"
VERSIONS_FILE = BENCH_DIR / "versions"
CURRENT_MACHINE_FILE = Path("/tmp/bench/current_machine")

TSP_BINARY = Path("./tsp")
TSP_INSTANCE = "dj38.tsp"

# ------------------------
# Utilities
# ------------------------


def ensure_dirs():
    for d in [BENCH_DIR, CONFIG_DIR, RESULTS_DIR, BIN_DIR, TMP_DIR]:
        d.mkdir(parents=True, exist_ok=True)


def load_machine_id():
    if CURRENT_MACHINE_FILE.exists():
        return CURRENT_MACHINE_FILE.read_text().strip()
    return None


def save_machine_id(mid: str):
    CURRENT_MACHINE_FILE.parent.mkdir(parents=True, exist_ok=True)
    CURRENT_MACHINE_FILE.write_text(mid)


def validate_machine_id(mid: str):
    return re.match(r"^[a-zA-Z0-9\-]+$", mid)


def prompt_machine_id():
    while True:
        mid = input(
            "Machine ID not found, please enter an ID for your machine: "
        ).strip()
        if validate_machine_id(mid):
            save_machine_id(mid)
            print(f"Saved '{mid}' as Machine ID.")
            return mid
        print("Invalid Machine ID (alphanumeric and dash only).")


def load_config(machine_id):
    cfg_path = CONFIG_DIR / f"{machine_id}.json"
    if not cfg_path.exists():
        print(f"Config file not found: {cfg_path}")
        sys.exit(1)
    with open(cfg_path) as f:
        cfg = json.load(f)
    # normalize keys
    cfg = {k.strip(): v for k, v in cfg.items()}
    return cfg


def setup_config(machine_id) -> dict:
    config_path = CONFIG_DIR / f"{machine_id}.json"
    """
    Interactively ask for configuration values and save them to config.json.
    """
    print("No configuration found. Let's set it up.\n")

    cities_raw = input("Enter cities counters (like 5,10,15): ").strip()
    cities = [int(c.strip()) for c in cities_raw.split(",") if c.strip()]

    while True:
        try:
            threads_raw = input("Enter threads counters: ").strip()
            threads = [int(c.strip()) for c in threads_raw.split(",") if c.strip()]
            break
        except ValueError:
            print("Threads must be a positive integer.")

    while True:
        try:
            cutoff_raw = input("Enter cutoff values: ").strip()
            cutoff = [int(c.strip()) for c in cutoff_raw.split(",") if c.strip()]
            break
        except ValueError:
            print("Cutoff must be a number.")

    config = {
        "machine_id": machine_id,
        "cities": cities,
        "threads": threads,
        "cutoff": cutoff,
    }

    config_path.parent.mkdir(parents=True, exist_ok=True)
    with config_path.open("w", encoding="utf-8") as f:
        json.dump(config, f, indent=4)

    print(f"\nConfiguration saved to {config_path}")
    return config


def get_git_hash():
    try:
        head = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
        dirty = subprocess.call(["git", "diff", "--quiet"]) != 0
        return head + ("+dirty" if dirty else "")
    except Exception:
        return "tmp"


def load_versions():
    if not VERSIONS_FILE.exists():
        return []
    lines = VERSIONS_FILE.read_text().splitlines()
    out = []
    for l in lines:
        name, git_hash, desc = l.split(" ", 2)
        out.append((name, git_hash, desc))
    return out


def latest_baseline():
    versions = load_versions()
    return versions[-1][0] if versions else None


def timestamp():
    return datetime.now().strftime("%Y-%m-%d_%H-%M-%S-%f")


def max_runs_for_cities(c):
    if c < 8:
        return None
    if 9 <= c <= 13:
        return 20
    return 2


def read_results(machine_id):
    rfile = RESULTS_DIR / machine_id / "results.json"
    if not rfile.exists():
        return []
    with open(rfile) as f:
        return json.load(f)


def write_results(machine_id, results):
    rdir = RESULTS_DIR / machine_id
    rdir.mkdir(parents=True, exist_ok=True)
    with open(rdir / "results.json", "w") as f:
        json.dump(results, f, indent=4)


def find_existing(results, baseline, c, t, co):
    for r in results:
        if (
            r["baseline"] == baseline
            and r["cities"] == c
            and r["threads"] == t
            and r["cutoff"] == co
        ):
            return r
    return None


def previous_baseline_time(results, baseline, c, t, co):
    versions = [v[0] for v in load_versions()]
    idx = versions.index(baseline)
    if idx == 0:
        return [None, None]
    prev = versions[idx - 1]
    for r in reversed(results):
        if (
            r["baseline"] == prev
            and r["cities"] == c
            and r["threads"] == t
            and r["cutoff"] == co
        ):
            return [r["mean"], r["baseline"]]
    return [None, None]


# ------------------------
# Commands
# ------------------------


def cmd_init(args):
    ensure_dirs()
    mid = load_machine_id()
    if not mid:
        prompt_machine_id()
    else:
        print(f"Current Machine ID: {mid}")

    if not (CONFIG_DIR / f"{mid}.json").exists():
        setup_config(mid)


def cmd_baseline(args):
    ensure_dirs()
    print("Building program with 'make'")
    subprocess.check_call(["make"])
    name = input("Give a name to the baseline: ").strip()
    desc = input("Give a description to this baseline: ").strip()
    git_hash = get_git_hash()
    print(f"Using Git hash: {git_hash}")

    target = BIN_DIR / f"tsp-{name}"
    shutil.copy2(TSP_BINARY, target)
    with open(VERSIONS_FILE, "a") as f:
        f.write(f"{name} {git_hash} {desc}\n")

    print(f"\nCreated baseline '{name}' !")


def cmd_run(args):
    ensure_dirs()
    mid = load_machine_id() or prompt_machine_id()
    cfg = load_config(mid)
    results = read_results(mid)

    versions = load_versions()
    if not versions:
        print("No baseline found.")
        sys.exit(1)

    default = versions[-1][0]
    print("Available baselines:")
    for i, (n, _, _) in enumerate(versions):
        print(f"{i + 1}. {n}")
    choice = input(f"Select baseline [default: {default}]: ").strip()
    baseline = default if not choice else versions[int(choice) - 1][0]

    print(colored(f"Executing baseline '{baseline}'\n", "blue"))

    combos = itertools.product(cfg["cities"], cfg["threads"], cfg["cutoff"])
    tsp_binary_for_baseline = BIN_DIR / f"tsp-{baseline}"

    if not tsp_binary_for_baseline.exists():
        print(f"Error: no file {tsp_binary_for_baseline} found")
        return 2

    sys.stdout.write("\n")
    sys.stdout.flush()

    for cities, threads, cutoff in combos:
        if find_existing(results, baseline, cities, threads, cutoff):
            continue

        mr = max_runs_for_cities(cities)
        out_json = TMP_DIR / f"{timestamp()}.json"

        print(
            colored(
                f"{cities} cities with {threads} threads and cutoff {cutoff}", "blue"
            ),
        )

        cmd = [
            "hyperfine",
            "-N",
            f"./{tsp_binary_for_baseline} {TSP_INSTANCE} {cities} {threads} {cutoff}",
            "--export-json",
            str(out_json),
        ]
        if mr:
            cmd.insert(2, "--max-runs")
            cmd.insert(3, str(mr))

        subprocess.call(cmd)

        # Try to clear the above lines generated by Hyperfine to have only the final value
        sys.stdout.write("\n")
        sys.stdout.flush()
        LINES_TO_CLEAR = 5
        for _ in range(LINES_TO_CLEAR):
            sys.stdout.write("\033[F")  # move cursor up one line
            sys.stdout.write("\033[K")  # clear line
        sys.stdout.flush()

        data = json.loads(out_json.read_text())
        mean = data["results"][0]["mean"]

        entry = {
            "mean": mean,
            "date": timestamp(),
            "baseline": baseline,
            "cities": cities,
            "threads": threads,
            "cutoff": cutoff,
        }
        results.append(entry)
        write_results(mid, results)

        prev_mean, prev_baseline = previous_baseline_time(
            results, baseline, cities, threads, cutoff
        )
        delta = ""
        if prev_mean:
            pct = ((mean - prev_mean) / prev_mean) * 100
            delta = colored(
                f"{pct:+.0f}% compared to '{prev_baseline}' ({prev_mean * 1000:.2f} ms)",
                "red" if pct < 0 else "green",
            )

        print(colored(f"{mean * 1000:.2f} ms   {delta}", "cyan"))


# ------------------------
# CLI
# ------------------------


def main():
    parser = argparse.ArgumentParser(prog="bench.py")
    sub = parser.add_subparsers()

    p_init = sub.add_parser("init")
    p_init.set_defaults(func=cmd_init)

    p_base = sub.add_parser("baseline")
    p_base.set_defaults(func=cmd_baseline)

    p_run = sub.add_parser("run")
    p_run.set_defaults(func=cmd_run)

    if len(sys.argv) == 1:
        parser.print_help()
        cmd_init(None)
        return

    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    try:
        main()
    except:
        print("stopped")
