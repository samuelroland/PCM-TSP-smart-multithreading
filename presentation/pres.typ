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

#blank-slide[
  #align(center, [
  #text(weight: "bold", size: 1.5em, fill: black)[TSP multithreading optimization]
    == Olivia Manz et Samuel Roland
    ==== Cours de PCM  - 2025
  ])
]

#slide(title: "Algorithm")[
#grid(columns: 2, column-gutter: 1em,
[
- AAAAAAAAAAAAAAAAAAAAA
- B
- C
],
[
  Amazing snippet

```cpp
if (argc < 2 || argc > 5) {
    std::cerr << "Usage: " << argv[0] << " <file.tsp> [nb cities] [nb threads] [cutoff]\n";
    return 1;
}
```

]
)
]

