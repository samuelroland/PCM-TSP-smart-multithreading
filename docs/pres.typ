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

#slide(title: "Open questions")[

We want to optimize for *any cities count* and *for the server*
+ What is the best cutoff for the server ?
+ What is the best threads number with cutoff of zero ?
+ What is the best threads number for the best cutoff ?
+ What core optimisations are going to improve the time for all cities ?
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

#slide(title: "Cutoff analysis - 256 threads")[
  #image("bench/plots/srv2-cutoff-analysis.svg", width: 100%)
]

#slide(title: "Threads analysis with cutoff at zero")[
  #image("bench/plots/srv2-threads-analysis-cutoff-zero.svg", width: 100%)
]

#slide(title: "Threads analysis with optimal cutoff at 8 on server")[
  #image("bench/plots/srv2-threads-analysis-cutoff-optimal.svg", width: 100%)
]

#slide(title: "Threads analysis with optimal cutoff at 8 on laptop")[
  #image("bench/plots/sam-threads-analysis.svg", width: 100%)
]

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
