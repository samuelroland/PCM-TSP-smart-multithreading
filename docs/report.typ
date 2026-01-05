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

The main ideas were to implement a lock free queue to replace the existing stack. This implementation also required to migrate part of the code to be safe in a multithreaded context. To further optimize the first results, we search for dedicated structures for this kind of problem and found the work-stealing deque, with the paper #quote("Dynamic circular work-stealing deque") @wsdpaper. We measured the speedup and efficiency of the final version. Along the way we measured several combinations of the 3 available parameters: number of cities, number of thread and cutoff.

TODO: résumé des résultats sans chiffres précis

= Data structures
In this section we present the structure of 2 core datastructures and one secondary structure also used.

== LockFreeQueue

== PriorityStack

== Work-stealing deque
The structure presented in the paper @wsdpaper, is a circular and growable buffer, that is used a double ended queue. One side of the queue (the bottom), each thread will be able to push and pop some tasks. As the other end (the top), some other threads might come to steal some tasks, if their own queue is empty.

As visible in @fig-wsd-diagram, like in the paper, the structure is using 2 classes. The `CircularArray` is the internal contiguous structure dynamically allocated, that can double its capacity when the buffer is full. It is using 2 indexes `bottom` and `top`, which are monotonic values (they always increment and never decrement). These indexes are used modulo the size of the buffer to avoid overflows and enable this circular style. Tasks are represented here with a number, to identify the portion of the buffer that is considered to be used.
#figure(
  image("schemas/wsd-basics.png", width: 100%),
  caption: [The abstract view of the 2 structures `CircularArray` and `CircularWSDeque`],
)
We are always sure that `top` is lower or equal to `bottom`. `top` is pointing on the top node but `bottom` is pointing on the next cell to be filled by a push on the bottom side. This is leaving an unused cell all the time.

#figure(
  image("imgs/wsd-diagram.png", width: 100%),
  caption: [The 2 classes used in the work-stealing implementation],
) <fig-wsd-diagram>




= Baseline

= Optimizations

= Perspectives
