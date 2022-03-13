#include <stdio.h>
#include "time.h"
#include "string.h"
#include <ubench/ubench.h>

#include "crafting_solver2.h"
#include "crafting_solver3.h"


UBENCH_EX(solver_perf2, culinarian_80) {
    auto ctx = Crafting_Solver2::init_solver_context(
        0,
        ALL_ACTIONS & ~CA_Manipulation & ~CA_Prudent_Synthesis & ~CA_Advanced_Touch & ~CA_Trained_Finesse,

        80,
        400,
        1645,
        1532,

        81,
        121,
        100,
        105,
        100,

        80,
        2000,
        5200
    );

    UBENCH_DO_BENCHMARK() {
        for(int i = 0; i < 100; i++)
            Crafting_Solver2::execute_round(ctx);
        UBENCH_DO_NOTHING(&ctx);
    }

}

UBENCH_EX(solver_perf3, culinarian_80) {
    auto ctx = Crafting_Solver3::init_solver_context(
        0,
        ALL_ACTIONS & ~CA_Manipulation & ~CA_Prudent_Synthesis & ~CA_Advanced_Touch & ~CA_Trained_Finesse,

        80,
        400,
        1645,
        1532,

        81,
        121,
        100,
        105,
        100,

        80,
        2000,
        5200
    );

    UBENCH_DO_BENCHMARK() {
        for (int i = 0; i < 100; i++)
            Crafting_Solver3::execute_round(ctx);
        UBENCH_DO_NOTHING(&ctx);
    }

}

UBENCH_MAIN();
