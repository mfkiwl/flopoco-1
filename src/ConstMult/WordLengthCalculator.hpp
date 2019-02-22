#ifndef WordLengthCalculator_HPP
#define WordLengthCalculator_HPP

#ifdef HAVE_PAGLIB

#include "pagsuite/adder_graph.h"
#include <gmpxx.h>

#endif // HAVE_PAGLIB

namespace flopoco {
    #ifdef HAVE_PAGLIB
    class WordLengthCalculator {

        public:
            WordLengthCalculator(PAGSuite::adder_graph_t adder_graph, 
                int wIn, 
                double epsilon) : adder_graph_(adder_graph), wIn_(wIn), epsilon_(epsilon) {}

            ~WordLengthCalculator() {}
            map<pair<mpz_class, int>, vector<int> > optimizeTruncation();

        private:
            PAGSuite::adder_graph_t adder_graph_;
            int wIn_;
            double epsilon_;
            map<pair<mpz_class, int>, vector<int> > truncationVal_;
    };
    #endif // HAVE_PAGLIB
} // namespace

#endif