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
#set page(margin: 1.5cm)

#set text(font: "Cantarell", size: 12pt)
#let figure = figure.with(
  kind: "image",
  supplement: none,
) // disable prefix in captions



= Multi-threading optimization for TSP

TODO slide page
Authors: Olivia Manz and Samuel Roland
PCM Course - 2025
#outline()

#pagebreak()

= Introduction

The goal of this project is to optimize the repartition of work among a large numbers of threads. The TSP has been chosen for this challenge, with a starting code implementing an existing single-threaded resolution. We had access to a 256 cores and 126GB of RAM machine remotely to test our program. The core of the work is on the internal lock-free data structures, that enable an optimized work repartition and synchronisation.

The main ideas were to implement a lock free queue to replace the existing stack. This implementation also required to migrate part of the code to be safe in a multithreaded context. Then, we implemented a work-stealing deque system, based on the paper #quote("Dynamic circular work-stealing deque") @wsdpaper. We measured the speedup and efficiency of the final version. Along the way we measured several combinations of the 3 available parameters: number of cities, number of thread and cutoff.

TODO: résumé des résultats sans chiffres précis
