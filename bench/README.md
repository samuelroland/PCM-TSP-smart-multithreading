# Benchmarking orchestration for the TSP

This tool `bench.py` is a Python wrapper on top of the Hyperfine CLI, to orchestrate the benchmarks, baselines, comparisons and plots generation for this TSP.

## Prerequisites
- `hyperfine` [Git repos](https://github.com/sharkdp/hyperfine)
- `uv` [website](https://docs.astral.sh/uv/)

## Commands

### init
Initial setup to configure your machine ID + decide on the local configuration for this machine.

```bash
uv run bench.py init
```

### run
Runs benchmarks on the **current code** and stores results in `bench/results/<machine>/fresh.json`

```bash
uv run bench.py run
```

### baseline new
Creates a **new baseline** by:
- asking for name and description
- building the code again via `make`
- runs benchmarks for the specific binary
- saves results as `bench/results/<machine id>/<baseline name>.json`
- copies the binary as `bench/bin/tsp-<name>`

```bash
uv run bench.py baseline new
```

### baseline save
Saves existing results (from previous `run` execution) from `fresh.json` as a baseline **without running benchmarks again**.

```bash
uv run bench.py baseline save
```

## Example Workflow
TODO
