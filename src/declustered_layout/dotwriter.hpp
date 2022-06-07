#pragma once

#include "graph.hpp"
#include "partitioning.hpp"

#include <array>
#include <cstdint>
#include <string>


namespace declustered_layout {


struct DotWriter {
    // https://graphs.grevian.org/example
    // dot -Tpng example.dot -o example.png

    static constexpr std::array colors = {"red", "blue", "green",
                                          "yellow", "pink", "lightblue",
                                          "forestgreen", "brown"};

    struct Render {
        std::string filename;

        void render();
    };

    static Render write(std::string filename, Graph& g, Partitioning& part);

    static Render write_directed(std::string filename, Graph& g);

    static Render write_clustered(std::string filename, Graph& g, Partitioning& part);
};


} // namespace declustered_layout
