#include "uci.hh"

namespace tc {

void uci_newgame(UCIState* state) {
    state->board = Board();
}

struct PerftStats {
    int leafTotalPseudoLegal = 0;
    int leafTotalLegal = 0;
};

template<bool turn>
void perft_branch(Board& b, PerftStats& s, int depth) {
    MoveList<NoOrderMoveOrderer, MAX_MOVES> moveList;
    gen_all_moves<decltype(moveList), movegenAllPL, turn>(&b, &moveList);

    if (depth == 0) {
        s.leafTotalPseudoLegal += moveList.count;
    }

    for (int i = moveList.count - 1; i >= 0; i--) {
        Move move = moveList.get_move(i);
        if (move.null()) continue;

        ExtMove<true> extMove(move);
        b.make_move_unchecked<turn, true>(&extMove);

        // check legal
        if (b.is_in_check<turn>()) {
            b.unmake_move_unchecked<turn, true>(&extMove);
            continue;
        }

        if (depth == 0) {
            s.leafTotalLegal++;
        }
        
        if (depth > 0) {
            perft_branch<!turn>(b, s, depth - 1);
        }

        b.unmake_move_unchecked<turn, true>(&extMove);
    }
}

void perft_print_row_content(Board& b, PerftStats& s) {
    std::cout << std::setfill(' ');
    std::cout << " | " << std::setw(12) << s.leafTotalLegal;
    std::cout << " | " << std::setw(12) << s.leafTotalPseudoLegal;
}

template<bool turn>
void perft_root_print(Board& b, int depth) {
    std::cout << "\n[*] Perft DEPTH " << depth << "\n\n";
    
    std::cout << "Move | Legal        | PseudoLegal    " << "\n";
    std::cout << "-----+--------------+----------------" << "\n";

    PerftStats allStats;

    MoveList<NoOrderMoveOrderer, MAX_MOVES> moveList;
    gen_all_moves<decltype(moveList), movegenAllPL, turn>(&b, &moveList);
    for (int i = moveList.count - 1; i >= 0; i--) {
        Move move = moveList.get_move(i);
        if (move.null()) continue;

        ExtMove<true> extMove(move);
        b.make_move_unchecked<turn, true>(&extMove);

        // check legal
        if (b.is_in_check<turn>()) {
            b.unmake_move_unchecked<turn, true>(&extMove);
            continue;
        }
        
        // perform perft
        PerftStats stats;
        perft_branch<!turn>(b, stats, depth - 1);
        b.unmake_move_unchecked<turn, true>(&extMove);

        // accumulate stats
        allStats.leafTotalLegal += stats.leafTotalLegal;
        allStats.leafTotalPseudoLegal += stats.leafTotalPseudoLegal;

        // print stats
        std::cout << FILE_TO_CHAR(FILE(move.src)) << RANK_TO_CHAR(RANK(move.src));
        std::cout << FILE_TO_CHAR(FILE(move.dst)) << RANK_TO_CHAR(RANK(move.dst));
        perft_print_row_content(b, stats);
        std::cout << "\n";
    }

    std::cout << " all";
    perft_print_row_content(b, allStats);
    std::cout << "\n";

    std::cout << "\n";
}

void perft_root_print_dyn(Board& b, int depth) {
    if (b.turn) perft_root_print<WHITE>(b, depth);
    else        perft_root_print<BLACK>(b, depth);
}

/*                                                       */
/* ============== Interface/UCI Main Loop ============== */
/*                                                       */

void uci_listen(UCIState* state, OptionParser* opt) {
    log<DEBUG>(P, "uci_listen()");
    
    /* Interface/UCI Main Loop */
    while (state->run) {
        std::cout << "> ";
        std::string str;
        std::getline(std::cin, str);

        // split by whitespace
        auto args = split_str_by_whitespace(str);
        if (args.size() == 0) {
            continue;
        }

        auto cmd = args[0];
        std::istringstream iss(str.substr(cmd.length()));
        std::noskipws(iss);
        std::istream_iterator<char> it(iss);
        std::istream_iterator<char> const end = { };
        skip_whitespace(it);

        /* ============ command handling ============ */

        // uci: uci
        if (cmd == "uci") {
            std::cout << "uciok\n";
            state->uci = true;
            continue;
        }

        // uci: debug
        if (cmd == "debug" && args.size() >= 2) {
            if (args[1] == "true") state->debug = true;
            else if (args[1] == "false") state->debug = false;

            std::cout << "debug = " << state->debug << "\n";
            continue;
        }

        // uci: isready
        if (cmd == "isready") {
            std::cout << "readyok\n";
        }

        // uci: ucinewgame
        if (cmd == "ucinewgame") {
            uci_newgame(state);
        }

        // uci: exit, e, quit, q
        if (cmd == "exit" || cmd == "e" || cmd == "quit" || cmd == "q") {
            log<DEBUG>(P, "Calling OS exit(0)");
            exit(0);
        }

        // uci: position
        if (cmd == "position" || cmd == "pos" || cmd == "p") {
            state->board.load_fen(it, end);
            debug_tostr_board(std::cout, state->board);
        }

        // uci: perft
        if (cmd == "perft") {
            int depth = parse_int(it, end);
            perft_root_print_dyn(state->board, depth);
        }
    }
}

}