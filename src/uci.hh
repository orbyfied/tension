#pragma once

#include <iostream>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <io.h>
#include <fcntl.h>

#include "util.hh"
#include "logging.hh"
#include "debug.hh"
#include "board.hh"
#include "search.hh"
#include "basiceval.hh"

#include "../vendor/popl/include/popl.hpp"

using namespace popl;

namespace tc {

struct UCIState {
    bool run = true;  // Whether to run the engine
    bool uci = false; // Whether UCI has been initialized
    bool debug = false;

    Board board;
};

/* Main UCI command loop */
void uci_listen(UCIState* state, OptionParser* opt);

}