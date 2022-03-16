#pragma once
#include "types.h"
#include "Craft_Actions.h"

namespace Crafting_Simulator {
    struct Context {
        s32x4 initial_state;
        f32x4 base_change;

        s32x4 state;
        // (Waste Not, Great Strides, Veneration, Innovation) (Manipulation, Muscle Memory, ---------, Inner Quiet)
        s16x8 turns;

        Craft_Action previous_action;
        s32 step;

        s32 player_level;
    };

    Context init_context(s32 player_level,
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
                         s32 quality);

    bool simulate_step(Context &context, Craft_Action action);
}