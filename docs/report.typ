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


= Multi-threading optimization for TSP
#outline()

== Introduction

== Algorithms

== Baseline

== Optimizations
