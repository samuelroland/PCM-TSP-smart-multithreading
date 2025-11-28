#include "tspgraph.hpp"
#include <iostream>

/*****************************************************************
  Example program to load and print a TSP graph
 *****************************************************************/

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <file.tsp>\n";
        return 1;
    }
    TSPGraph g(argv[1]);
    std::cout << g << std::endl;
    return 0;
}
