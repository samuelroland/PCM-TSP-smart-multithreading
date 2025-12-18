#!/usr/bin/env python3
# NOTE: this is vibe coded with ChatGPT, and adapted for small fixes.
import os
import traceback
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
FRESH_NAME = "fresh"
BIN_DIR = BENCH_DIR / "bin"
TMP_DIR = BENCH_DIR / "tmp"
VERSIONS_FILE = BENCH_DIR / "versions"
CURRENT_MACHINE_FILE = Path("/tmp/bench/current_machine")

TSP_BINARY = Path("./tsp")
TSP_INSTANCE = "dj38.tsp"

# ------------------------
# Utilities
# ------------------------


def all_combinations(cfg):
    return list(itertools.product(cfg["cities"], cfg["threads"], cfg["cutoff"]))


def existing_combinations(results):
    return {(r["cities"], r["threads"], r["cutoff"]) for r in results}


def ensure_baseline_binary(baseline, git_hash):
    target = BIN_DIR / f"tsp-{baseline}"
    if target.exists():
        print(colored(f"[OK] Binary exists for '{baseline}'", "green"))
        return True

    print(colored(f"[BUILD] Creating binary for '{baseline}'", "blue"))

    clone_dir = BENCH_DIR / "selfclone"
    if clone_dir.exists():
        shutil.rmtree(clone_dir)

    try:
        subprocess.check_call(["git", "clone", ".git", str(clone_dir)])
        subprocess.check_call(["git", "checkout", git_hash], cwd=clone_dir)
        subprocess.check_call(["make"], cwd=clone_dir)

        built = clone_dir / "tsp"
        if not built.exists():
            raise RuntimeError("tsp binary not produced by make")

        shutil.copy2(built, target)
        print(colored(f"[OK] Built tsp-{baseline}", "green"))
        return True

    except Exception as e:
        print(colored(f"[FAIL] Could not build '{baseline}': {e}", "red"))
        return False

    finally:
        if clone_dir.exists():
            shutil.rmtree(clone_dir)


def build():
    try:
        subprocess.check_call(["make"])
    except Exception as e:
        print(e)


def format_duration(seconds: float) -> str:
    """
    Format a duration given in seconds into ms, s, or min.

    Examples:
    0.42    -> "420.00 ms"
    2.1059 -> "2.11 s"
    75.3   -> "1.26 min"
    """

    if seconds < 0.001:
        return f"{seconds * 1000:.4f} ms"
    elif seconds < 1:
        return f"{seconds * 1000:.2f} ms"
    elif seconds < 60:
        return f"{seconds:.2f} s"
    else:
        return f"{seconds / 60:.2f} min"


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
        return 300
    if c <= 12:
        return 200
    if c < 14:
        return 40
    if c < 16:
        return 5
    if c < 17:
        return 2
    return 1  # above 16 it's getting very slow with best parameters


def results_file(machine_id, baseline):
    return RESULTS_DIR / machine_id / f"{baseline}.json"


def read_results(machine_id, baseline):
    rfile = results_file(machine_id, baseline)
    if not rfile.exists():
        return []
    with open(rfile) as f:
        return json.load(f)


def write_results(machine_id, baseline, results):
    rdir = RESULTS_DIR / machine_id
    rdir.mkdir(parents=True, exist_ok=True)
    with open(results_file(machine_id, baseline), "w") as f:
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


def previous_baseline_time(baseline, machine_id, c, t, co):
    versions = [v[0] for v in load_versions()]
    prev = latest_baseline()
    if not prev:
        return [None, None]

    if baseline != FRESH_NAME:
        if baseline not in versions:
            return [None, None]
        idx = versions.index(baseline)
        if idx == 0:
            return [None, None]
        prev = versions[idx - 1]

    # Load results from defined baseline
    results = json.loads(((RESULTS_DIR / machine_id) / f"{prev}.json").read_text())
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


def cmd_init():
    ensure_dirs()
    mid = load_machine_id()
    if not mid:
        prompt_machine_id()
    else:
        print(f"Current Machine ID: {mid}")

    if not (CONFIG_DIR / f"{mid}.json").exists():
        setup_config(mid)


def cmd_baseline():
    ensure_dirs()
    print(
        "baseline command:\n"
        "  baseline new   -> run benchmarks and save as a new baseline\n"
        "  baseline save  -> save current fresh results as a baseline\n"
    )


def cmd_baseline_new(args):
    ensure_dirs()
    build()

    name = input("Baseline name: ").strip()
    desc = input("Baseline description: ").strip()
    git_hash = get_git_hash()

    target = BIN_DIR / f"tsp-{name}"
    shutil.copy2(TSP_BINARY, target)

    # save line after that to avoid comparison with itself
    with open(VERSIONS_FILE, "a") as f:
        f.write(f"{name} {git_hash} {desc}\n")

    # run benchmarks immediately
    args.baseline = name
    cmd_run(args)


def cmd_baseline_save():
    ensure_dirs()
    mid = load_machine_id() or prompt_machine_id()

    fresh = results_file(mid, FRESH_NAME)
    if not fresh.exists():
        print("Error: no fresh results to save.")
        sys.exit(1)

    name = input("Baseline name: ").strip()
    desc = input("Baseline description: ").strip()
    git_hash = get_git_hash()

    new_result_file = results_file(mid, name)
    shutil.copy2(fresh, new_result_file)
    # hacky way to rename baseline: fresh -> baseline: <newname> inside the JSON
    # without iterating on the JSON array inside. Should be mostly safe as long as dates or numbers cannot contain the baseline name.
    with open(new_result_file, "r", encoding="utf-8") as file:
        text = file.read()
    text = text.replace("fresh", name)
    with open(new_result_file, "w", encoding="utf-8") as file:
        file.write(text)
    shutil.copy2(TSP_BINARY, BIN_DIR / f"tsp-{name}")

    with open(VERSIONS_FILE, "a") as f:
        f.write(f"{name} {git_hash} {desc}\n")

    print(f"Saved fresh results as baseline '{name}'")


# TODO: refactor common code with cmd_run
def run_missing_for_baseline(baseline, git_hash):
    mid = load_machine_id() or prompt_machine_id()
    cfg = load_config(mid)

    expected = set(all_combinations(cfg))
    results = read_results(mid, baseline)
    done = existing_combinations(results)
    missing = expected - done

    if not missing:
        print(colored(f"[SKIP] '{baseline}' already complete", "green"))
        return

    if not ensure_baseline_binary(baseline, git_hash):
        return

    tsp_binary = BIN_DIR / f"tsp-{baseline}"

    print(colored(f"[RUN] Completing '{baseline}' ({len(missing)} missing)", "blue"))

    for cities, threads, cutoff in sorted(missing):
        mr = max_runs_for_cities(cities)
        out_json = TMP_DIR / f"{timestamp()}.json"

        cmd = [
            "hyperfine",
            "-N",
            f"./{tsp_binary} {TSP_INSTANCE} {cities} {threads} {cutoff}",
            "--export-json",
            str(out_json),
        ]
        if mr:
            cmd.insert(2, "--max-runs")
            cmd.insert(3, str(mr))

        if cities >= 10:
            cmd.insert(
                2,
                "--prepare",
            )
            cmd.insert(
                3, "sleep 1"
            )  # because some of the commands are taking increasingly more time when run in loop without any break

        subprocess.call(cmd)

        try:
            data = json.loads(out_json.read_text())
            mean = data["results"][0]["mean"]

            results.append(
                {
                    "mean": mean,
                    "date": timestamp(),
                    "baseline": baseline,
                    "cities": cities,
                    "threads": threads,
                    "cutoff": cutoff,
                }
            )

            write_results(mid, baseline, results)
            print(
                colored(
                    f"{baseline}: {cities}/{threads}/{cutoff} -> {format_duration(mean)}",
                    "cyan",
                )
            )

        except Exception as e:
            print(colored(f"[ERROR] Ignored result: {e}", "red"))


def cmd_complete():
    ensure_dirs()
    mid = load_machine_id() or prompt_machine_id()
    cfg = load_config(mid)

    versions = load_versions()
    expected_count = len(all_combinations(cfg))

    incomplete = []

    print("\nBaseline status:\n")

    for idx, (name, git_hash, _) in enumerate(versions):
        rfile = results_file(mid, name)
        if not rfile.exists():
            print(f"[{idx}] {name:<8} : no results")
            incomplete.append((idx, name, git_hash))
            continue

        results = read_results(mid, name)
        found = len(existing_combinations(results))

        if found < expected_count:
            print(f"[{idx}] {name:<8} : incomplete ({found}/{expected_count})")
            incomplete.append((idx, name, git_hash))
        else:
            print(f"[{idx}] {name:<8} : complete ({found}/{expected_count})")

    if not incomplete:
        print(colored("\nAll baselines are complete.", "green"))
        return

    choice = input(
        "\nSelect baseline number to complete, or 'a' for all incomplete: "
    ).strip()

    if choice.lower() == "a":
        selected = incomplete
    else:
        try:
            idx = int(choice)
            selected = [b for b in incomplete if b[0] == idx]
            if not selected:
                raise ValueError()
        except ValueError:
            print("Invalid selection.")
            return

    for _, name, git_hash in selected:
        run_missing_for_baseline(name, git_hash)


def cmd_run(args):
    build()
    ensure_dirs()
    mid = load_machine_id() or prompt_machine_id()
    cfg = load_config(mid)
    baseline = getattr(args, "baseline", FRESH_NAME)
    results = []

    tsp_binary_for_baseline = (
        BIN_DIR / f"tsp-{baseline}" if baseline != FRESH_NAME else TSP_BINARY
    )
    if baseline == FRESH_NAME:
        print(colored("Executing benchmarks on current code", "blue"))
    else:
        print(colored(f"Executing baseline '{baseline}'", "blue"))

    combos = itertools.product(cfg["cities"], cfg["threads"], cfg["cutoff"])

    sys.stdout.write("\n")
    sys.stdout.flush()

    for cities, threads, cutoff in combos:
        # if find_existing(results, baseline, cities, threads, cutoff):
        #     continue

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

        try:
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
            write_results(mid, baseline, results)

            prev_mean, prev_baseline = previous_baseline_time(
                baseline, mid, cities, threads, cutoff
            )
            delta = ""
            if prev_mean:
                pct = ((mean - prev_mean) / prev_mean) * 100
                delta = colored(
                    f"{pct:+.0f}% since '{prev_baseline}' ({format_duration(prev_mean)})",
                    "green" if pct < 0 else "yellow" if pct <= 6 else "red",
                )

            print(colored(f"{format_duration(mean)}    {delta}", "cyan"))

        except Exception as e:
            print(f"Exception {e} caused the result to be ignored...")


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
    sub_baseline = p_base.add_subparsers()
    p_base_new = sub_baseline.add_parser("new")
    p_base_new.set_defaults(func=cmd_baseline_new)

    p_base_save = sub_baseline.add_parser("save")
    p_base_save.set_defaults(func=cmd_baseline_save)

    p_run = sub.add_parser("run")
    p_run.set_defaults(func=cmd_run)

    p_complete = sub.add_parser("complete")
    p_complete.set_defaults(func=cmd_complete)

    if len(sys.argv) == 1:
        parser.print_help()
        cmd_init()
        return

    args = parser.parse_args()
    args.func()


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        tb = traceback.extract_tb(e.__traceback__)
        this_file = os.path.abspath(__file__)

        print(f"{type(e).__name__}: {e}")
        # find the last traceback entry from *this* file
        for frame in reversed(tb):
            if os.path.abspath(frame.filename) == this_file:
                print(f"Stopped at line {frame.lineno}: {frame.line}")
                break
