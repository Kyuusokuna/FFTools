#pragma once
#include <stdint.h>
#include "types.h"
#include "Craft_Actions.h"
#include "crafting_simulator.h"

#define MAX_SOLVE_LENGTH (45)

namespace Crafting_Solver3 {
    struct Result {
        u64 seed;
        s32x4 state;
        int depth;
        Craft_Action actions[MAX_SOLVE_LENGTH];
    };

    struct Solver_Context {
        Crafting_Simulator::Context sim_context;
        Result current_best;

        u64 seed;
        u64 rng_inc;
        u32 active_actions;

        Craft_Action active_actions_lookup_table[32];
    };

    Solver_Context init_solver_context(
        u32 seed,
        u32 active_actions,

        s32 player_level,
        s32 cp,
        s32 craftsmanship,
        s32 control,

        s32 recipe_level,
        s32 progress_divider,
        s32 progress_modifier,
        s32 quality_divider,
        s32 quality_modifier,

        s32 durability,
        s32 progress,
        s32 quality
    );

    void execute_round(Solver_Context &context);
    bool b_better_than_a(Result a, Result b);

}