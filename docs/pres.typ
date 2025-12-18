#import "@preview/typslides:1.2.3": * // https://github.com/manjavacas/typslides

// Project configuration
#show: typslides.with(
  ratio: "16-9",
  theme: "dusky",
)

#show raw.where(block: false): box.with(
    fill: luma(240),
    inset: (x: 3pt, y: 0pt),
    outset: (y: 3pt),
    radius: 2pt,
)
#show raw.where(block: true): block.with(
    inset: 10pt,
    radius: 2pt,
    stroke: 1pt + luma(200)
)

#set text(font: "Cantarell")
#let figure = figure.with(
  kind: "image",
  supplement: none,
) // disable prefix in captions

// #blank-slide[
//   #align(center, [
//   #text(weight: "bold", size: 1.5em, fill: black)[Multi-threading optimization for TSP]
//     == Olivia Manz and Samuel Roland
//     ==== Cours de PCM  - 2025
//   ])
// ]

#front-slide(authors: "Olivia Manz and Samuel Roland", info: "Cours de PCM - 2025", title: "Multi-threading optimization for TSP")

#slide(title: "Our core structure")[
  - A simple `LockFreeQueue` based on the course
  - A small tweak in `split()`:
    #text(size: 0.9em)[
    ```cpp
    if (_path.distance() >= _shortest.distance()) return -1;// the branch must be cut
    ```
    ]
    -> skip call to `solve()` and directly call `merge()`
  - called the "base" version
]

#slide(title: "Open questions")[

We want to optimize for *any cities count* and *for the server*
+ What is the best general cutoff ?
+ What is the best threads number with cutoff of zero ?
+ What is the best threads number for the best cutoff ?
+ What core optimisations can improve the time ?
]

#slide(title: "Benchmarking with Hyperfine")[
  #text(size: 0.9em)[

```sh
> ./tsp
Usage: ./tsp <file.tsp> [nb cities] [nb threads] [cutoff]
```
```sh
> hyperfine './tsp dj38.tsp 12 50 4' --export-json out.json
Benchmark 1: ./tsp dj38.tsp 12 50 4
  Time (mean ± σ):     745.4 ms ±  77.3 ms 
  Range (min … max):   640.8 ms … 861.4 ms 10 runs
```


```json
{
  "results": [
    {
      "command": "./tsp dj38.tsp 12 50 4",
      "mean": 0.745369825,
      "min": 0.6407724576,
      "max": 0.8614248116000001,
      ...
```
  ]
]

#slide(title: "Benchmarking orchestration system")[

  #text(size: 0.9em)[
```console
> uv run bench.py init
Machine ID not found, please enter an ID for your machine: srv2
Saved 'srv2' as Machine ID.
No configuration found. Let's set it up.

Enter cities counters (like 5,10,15): 5,10,13
Enter threads counters: 256
Enter cutoff values: 0,1,2,3,4,5,6,7,8,9,10,11,12,13

Configuration saved to bench/configs/srv2.json
```

```json
{
    "machine_id": "srv2",
    "cities": [ 5, 10, 13 ],
    "threads": [ 256 ],
    "cutoff": [ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 ]
}
```
]
]

#slide(title: "Benchmarking orchestration system")[

  #text(size: 0.9em)[
#grid(columns: 2, column-gutter: 1em, align: top,
[
  Baseline creation
```console
> uv run bench.py baseline new
make: Nothing to be done for 'all'.
Baseline name: base
Baseline description: Starting code
Executing baseline 'base'

12 cities with 12 threads and cutoff 8
6.52 ms
12 cities with 12 threads and cutoff 12
24.66 ms
12 cities with 50 threads and cutoff 8
13.83 ms
...
```

],[
Execution with comparisons
#image("imgs/bench-run-cmp.png", width: 100%)
]
)
]
]

#slide(title: "Cutoff analysis - 256 threads - on server")[
  #image("bench/plots/srv2-cutoff-analysis.svg", width: 100%)
]

#slide(title: "Threads analysis with cutoff at zero - on server")[
  #image("bench/plots/srv2-threads-analysis-cutoff-zero.svg", width: 100%)
]

#slide(title: "Threads analysis with optimal cutoff at 8 - on server")[
  #image("bench/plots/srv2-threads-analysis-cutoff-optimal.svg", width: 100%)
]

#slide(title: "Threads analysis with optimal cutoff at 8 - on laptop")[
  #image("bench/plots/sam-threads-analysis.svg", width: 100%)
]

#slide(title: "Optimal parameters ?")[
  - 30 and 256 threads
  - cutoff to 8
]

#slide(title: "Optimisation 1")[
  - Named "Keep first task in your hand" in `recurse()`

  #text(size: 0.9em)[
  ```diff
          if (n > 0) {
              _splits++;
  -            for (int i = 0; i < n; i++) {
  +            // keep the first task selfishly
  +            Task* next_local = coll[0];
  +            for (int i = 1; i < n; i++) {
                  Task* sub = coll[i];
                  enqueue(sub);
              }
              delete t;
  +            // continue with local task
  +            recurse(next_local);
          } else {
              _solves++;
              t->solve();
  ```
  ]
]

#slide(title: "Optimisation 2")[
  - Named "Take first task heuristic"
  ```cpp
  // actual distance - path already done > probably a better way
  estimated_cost = _path.distance() - _path.size();
  ```
]
#slide(title: "Optimisations results - on server")[
  #image("bench/plots/srv2-baseline-cmp.svg", width: 100%)
]

#slide(title: "Optimisations results - on laptop")[
  #image("bench/plots/sam-baseline-cmp.svg", width: 100%)
]



// #slide(title: "Perspectives")[
// ```sh
// > hyperfine './tsp-direct dj38.tsp 12'
// Benchmark 1: ./tsp-direct dj38.tsp 12
//   Time (mean ± σ):      67.8 ms ±   3.4 ms    [User: 66.3 ms, System: 0.9 ms]
//   Range (min … max):    62.2 ms …  76.9 ms    46 runs
//
// > hyperfine './bench/bin/tsp-base dj38.tsp 12 15 8'
// Benchmark 1: ./bench/bin/tsp-base dj38.tsp 12 15 8
//   Time (mean ± σ):      53.1 ms ±  43.3 ms    [User: 465.3 ms, System: 92.3 ms]
//   Range (min … max):    16.4 ms … 260.0 ms    106 runs
//
// > hyperfine './bench/bin/tsp-takefirsttaskwithheuristic dj38.tsp 12 15 8'
// Benchmark 1: ./bench/bin/tsp-takefirsttaskwithheuristic dj38.tsp 12 15 8
//   Time (mean ± σ):      41.2 ms ±  37.9 ms    [User: 366.3 ms, System: 60.4 ms]
//   Range (min … max):    12.7 ms … 167.3 ms    170 runs
// ```
// - 67.8ms -> 53.1ms -> 1.27x
// - 67.8ms -> 41.2ms -> 1.6x
// ]


// #slide(title: "Final comparisons")[
// ```sh
// > hyperfine './tsp-direct dj38.tsp 16'
// 25.263 s
//
// > hyperfine './bench/bin/tsp-base dj38.tsp 12 15 8'
// worst
//
// > hyperfine './bench/bin/tsp-takefirsttaskwithheuristic dj38.tsp 12 15 8'
// 19.561 s 
// ```
// - 67.8ms -> 53.1ms -> 1.27x
// - 67.8ms -> 41.2ms -> 1.6x
// ]

// #slide(title: "Algorithm")[
// #grid(columns: 2, column-gutter: 1em,
// [
// - AAAAAAAAAAAAAAAAAAAAA
// - B
// - C
// ],
// [
//   Amazing snippet
//
// ```cpp
// if (argc < 2 || argc > 5) {
//     std::cerr << "Usage: " << argv[0] << " <file.tsp> [nb cities] [nb threads] [cutoff]\n";
//     return 1;
// }
// ```
//
// ]
// )
// ]
//
