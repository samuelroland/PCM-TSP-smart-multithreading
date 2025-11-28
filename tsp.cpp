#include "tsptask.hpp"
#include <iostream>

/*****************************************************************
  Program to solve a TSP problem
  Arguments: tsp <filename> [number]
             filename: file to load the TSP graph from
             number: size of the graph (resized from the file)
  The program uses a TSPTask to solve the TSP problem, using
  two runners: DirectTaskRunner directly calls solve(), and
  PartitionedTaskStackRunner calls split(), then recurse in all
  partitions, then call merge().
 *****************************************************************/

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " <file.tsp> [number]\n";
        return 1;
    }

    TSPGraph graph(argv[1]);
    if (argc == 3)
        graph.resize(atoi(argv[2]));

    TSPPath::setup(&graph);

    TSPTask tsp2;
    DirectTaskRunner r2;
    r2.run(&tsp2);
    std::cout << "direct: " << tsp2.result() << " t:" << r2.duration() << std::endl;

    TSPTask tsp1;
    tsp1.cutoff(0);
    PartitionedTaskStackRunner r1(TSPPath::MAX_GRAPH);
    r1.run(&tsp1);
    std::cout << "partit: " << tsp1.result() << " t:" << r1.duration()
              << " s:" << r1.solves() << "/" << r1.splits() << std::endl;

    return 0;
}
