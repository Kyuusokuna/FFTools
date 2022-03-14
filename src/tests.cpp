#include <utest/utest.h>
#include "crafting_simulator.h"
#include "../generated_src/compressed_data.h"

using namespace Crafting_Simulator;

UTEST_STATE();
int main(int argc, char **argv) {
    if (!decompress_all_data()) {
        printf("ERROR: Failed to decompress necessary data for tests.\n");
        return 1;
    }

    return utest_main(argc, argv);
}

Recipe *get_recipe_by_name(Craft_Job job, ROString name) {
    for (auto &recipe : Recipes[job]) {
        if (!recipe.result)
            break;

        auto recipe_name = Item_to_name[recipe.result];
        if (recipe_name == name)
            return &recipe;
    }

    return 0;
}

struct SimpleResult {
    s32 cp;
    s32 durability;
    s32 progress;
    s32 quality;
};

#define SCRIP_70 400, 1645, 1532

Context make_context(s32 level, s32 cp, s32 craftsmanship, s32 control, Recipe *recipe) {
    return init_context(
        level,
        cp,
        craftsmanship,
        control,

        recipe->level,
        recipe->progress_divider,
        recipe->progress_modifier,
        recipe->quality_divider,
        recipe->quality_modifier,

        recipe->durability,
        recipe->progress,
        recipe->quality
    );
}

#define CHECK_STATE(state, result)                              \
    ASSERT_EQ((state).x, (result).cp);                          \
    ASSERT_EQ((state).y, (result).durability);                  \
    ASSERT_EQ(recipe->progress - (state).z, (result).progress); \
    ASSERT_EQ(recipe->quality - (state).w, (result).quality); 

#define SIMPLE_EVAL_AND_CHECK(level, stats, recipe, actions, result) {  \
    auto context = make_context((level), stats, (recipe));              \
                                                                        \
    for (auto action : (actions))                                       \
        ASSERT_TRUE(simulate_step(context, action));                    \
                                                                        \
    ASSERT_EQ(context.step, ARRAYSIZE((actions)));                      \
    CHECK_STATE(context.state, (result));                               \
}



UTEST(Action, Muscle_memory) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");

    {
        Craft_Action actions[] = { CA_Muscle_memory, CA_Basic_Touch, CA_Basic_Touch, CA_Basic_Touch, CA_Basic_Touch, CA_Basic_Synthesis, CA_Basic_Synthesis };
        SimpleResult result = { .cp = 322, .durability = 10, .progress = 903, .quality = 827 };

        SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
    }

    {
        Craft_Action actions[] = { CA_Muscle_memory, CA_Basic_Touch, CA_Basic_Touch, CA_Basic_Touch, CA_Basic_Touch, CA_Basic_Touch, CA_Basic_Synthesis };
        SimpleResult result = { .cp = 304, .durability = 10, .progress = 575, .quality = 1079 };

        SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
    }
}

UTEST(Action, Reflect) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Reflect, CA_Basic_Touch };
    SimpleResult result = { .cp = 376, .durability = 60, .progress = 0, .quality = 396 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}

UTEST(Action, Trained_Eye) {
    // TODO
}


UTEST(Action, Basic_Synthesis) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Basic_Synthesis };
    SimpleResult result = { .cp = 400, .durability = 70, .progress = 164, .quality = 0 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}

UTEST(Action, Careful_Synthesis) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Careful_Synthesis };
    SimpleResult result = { .cp = 393, .durability = 70, .progress = 205, .quality = 0 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}

UTEST(Action, Prudent_Synthesis) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Prudent_Synthesis };
    SimpleResult result = { .cp = 382, .durability = 75, .progress = 246, .quality = 0 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}

UTEST(Action, Focused_Synthesis) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    SimpleResult result = { .cp = 388, .durability = 70, .progress = 274, .quality = 0 };

    auto context = make_context(80, SCRIP_70, recipe);

    ASSERT_FALSE(simulate_step(context, CA_Focused_Synthesis));
    ASSERT_TRUE(simulate_step(context, CA_Observe));
    ASSERT_TRUE(simulate_step(context, CA_Focused_Synthesis));

    ASSERT_EQ(context.step, 2);
    CHECK_STATE(context.state, result);
}

UTEST(Action, Delicate_Synthesis) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Delicate_Synthesis };
    SimpleResult result = { .cp = 368, .durability = 70, .progress = 137, .quality = 180 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}

UTEST(Action, Groundwork) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Groundwork };
    SimpleResult result = { .cp = 382, .durability = 60, .progress = 411, .quality = 0 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}


UTEST(Action, Basic_Touch) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Basic_Touch };
    SimpleResult result = { .cp = 382, .durability = 70, .progress = 0, .quality = 180 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}

UTEST(Action, Standard_Touch) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Standard_Touch };
    SimpleResult result = { .cp = 368, .durability = 70, .progress = 0, .quality = 225 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}

UTEST(Action, Advanced_Touch) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Advanced_Touch };
    SimpleResult result = { .cp = 354, .durability = 70, .progress = 0, .quality = 270 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}

UTEST(Action, Prudent_Touch) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Prudent_Touch };
    SimpleResult result = { .cp = 375, .durability = 75, .progress = 0, .quality = 180 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}

UTEST(Action, Focused_Touch) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    SimpleResult result = { .cp = 375, .durability = 70, .progress = 0, .quality = 270 };

    auto context = make_context(80, SCRIP_70, recipe);

    ASSERT_FALSE(simulate_step(context, CA_Focused_Touch));
    ASSERT_TRUE(simulate_step(context, CA_Observe));
    ASSERT_TRUE(simulate_step(context, CA_Focused_Touch));

    ASSERT_EQ(context.step, 2);
    CHECK_STATE(context.state, result);
}

UTEST(Action, Preparatory_Touch) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Preparatory_Touch };
    SimpleResult result = { .cp = 360, .durability = 60, .progress = 0, .quality = 360 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}


UTEST(Action, Byregots_Blessing) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    SimpleResult result = { .cp = 370, .durability = 60, .progress = 0, .quality = 482 };

    auto context = make_context(80, SCRIP_70, recipe);

    ASSERT_FALSE(simulate_step(context, CA_Byregots_Blessing));
    ASSERT_TRUE(simulate_step(context, CA_Reflect));
    ASSERT_TRUE(simulate_step(context, CA_Byregots_Blessing));
    ASSERT_EQ(context.turns.e1.w, 0);

    ASSERT_EQ(context.step, 2);
    CHECK_STATE(context.state, result);
}

UTEST(Action, Trained_Finesse) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    SimpleResult result = { .cp = 102, .durability = 20, .progress = 0, .quality = 2951 };

    auto context = make_context(80, SCRIP_70, recipe);

    ASSERT_FALSE(simulate_step(context, CA_Trained_Finesse));
    ASSERT_TRUE(simulate_step(context, CA_Reflect));
    ASSERT_FALSE(simulate_step(context, CA_Trained_Finesse));
    ASSERT_TRUE(simulate_step(context, CA_Waste_Not2));

    for (int i = 0; i < 8; i++) {
        ASSERT_FALSE(simulate_step(context, CA_Trained_Finesse));
        ASSERT_TRUE(simulate_step(context, CA_Basic_Touch));
    }

    ASSERT_EQ(context.turns.e1.w, 10);
    ASSERT_TRUE(simulate_step(context, CA_Basic_Touch));
    ASSERT_EQ(context.turns.e1.w, 10);
    ASSERT_TRUE(simulate_step(context, CA_Trained_Finesse));
    ASSERT_EQ(context.turns.e1.w, 10);

    ASSERT_EQ(context.step, 12);
    CHECK_STATE(context.state, result);
}


UTEST(Combos, Basic_Standard_Touch) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Basic_Touch, CA_Standard_Touch };
    SimpleResult result = { .cp = 364, .durability = 60, .progress = 0, .quality = 427 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}

UTEST(Combos, Basic_Standard_Advanced_Touch) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Basic_Touch, CA_Standard_Touch, CA_Advanced_Touch };
    SimpleResult result = { .cp = 346, .durability = 50, .progress = 0, .quality = 751 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}

UTEST(Combos, Standard_Advanced_Touch) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Standard_Touch, CA_Advanced_Touch };
    SimpleResult result = { .cp = 350, .durability = 60, .progress = 0, .quality = 522 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}

UTEST(Combos, Standard_Standard_Touch) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Standard_Touch, CA_Standard_Touch };
    SimpleResult result = { .cp = 336, .durability = 60, .progress = 0, .quality = 472 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}

UTEST(Combos, Advanced_Advanced_Touch) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Advanced_Touch, CA_Advanced_Touch };
    SimpleResult result = { .cp = 308, .durability = 60, .progress = 0, .quality = 567 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}



UTEST(Buffs, Masters_Mend) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Basic_Touch, CA_Basic_Touch, CA_Basic_Touch, CA_Basic_Touch, CA_Masters_Mend };
    SimpleResult result = { .cp = 240, .durability = 70, .progress = 0, .quality = 827 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}

UTEST(Buffs, Waste_Not) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Waste_Not, CA_Basic_Synthesis, CA_Basic_Touch, CA_Basic_Touch, CA_Basic_Touch, CA_Basic_Touch };
    SimpleResult result = { .cp = 272, .durability = 50, .progress = 164, .quality = 827 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}

UTEST(Buffs, Waste_Not2) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Waste_Not2, CA_Basic_Synthesis, CA_Basic_Touch, CA_Basic_Touch, CA_Basic_Touch, CA_Basic_Touch, CA_Basic_Touch, CA_Basic_Touch, CA_Basic_Touch, CA_Basic_Touch };
    SimpleResult result = { .cp = 158, .durability = 30, .progress = 164, .quality = 1943 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}

UTEST(Buffs, Manipulation) {
    // TODO
}

UTEST(Buffs, Veneration) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Veneration, CA_Basic_Touch, CA_Basic_Synthesis, CA_Basic_Synthesis, CA_Basic_Synthesis, CA_Basic_Synthesis };
    SimpleResult result = { .cp = 364, .durability = 30, .progress = 902, .quality = 180 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}

UTEST(Buffs, Great_Strides) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");

    {
        Craft_Action actions[] = { CA_Great_Strides, CA_Basic_Touch, CA_Basic_Touch };
        SimpleResult result = { .cp = 332, .durability = 60, .progress = 0, .quality = 558 };

        SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
    }

    {
        Craft_Action actions[] = { CA_Great_Strides, CA_Basic_Synthesis };
        SimpleResult result = { .cp = 368, .durability = 70, .progress = 164, .quality = 0 };

        SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
    }

    {
        Craft_Action actions[] = { CA_Great_Strides, CA_Basic_Synthesis, CA_Basic_Synthesis, CA_Basic_Touch, };
        SimpleResult result = { .cp = 350, .durability = 50, .progress = 328, .quality = 360 };

        SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
    }

    {
        Craft_Action actions[] = { CA_Great_Strides, CA_Basic_Synthesis, CA_Basic_Synthesis, CA_Basic_Synthesis, CA_Basic_Touch };
        SimpleResult result = { .cp = 350, .durability = 40, .progress = 492, .quality = 180 };

        SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
    }
}

UTEST(Buffs, Innovation) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");
    Craft_Action actions[] = { CA_Innovation, CA_Basic_Synthesis, CA_Basic_Touch, CA_Basic_Touch, CA_Basic_Touch, CA_Basic_Touch };
    SimpleResult result = { .cp = 310, .durability = 30, .progress = 164, .quality = 1124 };

    SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
}



UTEST(Manual, buff_rounding) {
    Recipe *recipe = get_recipe_by_name(Craft_Job_CUL, "Rarefied Archon Loaf");

    {
        Craft_Action actions[] = { CA_Innovation, CA_Delicate_Synthesis, CA_Delicate_Synthesis, CA_Delicate_Synthesis, };
        SimpleResult result = { .cp = 286, .durability = 50, .progress = 411, .quality = 891 };
        SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
    }

    {
        Craft_Action actions[] = { CA_Delicate_Synthesis, CA_Delicate_Synthesis, CA_Delicate_Synthesis, CA_Delicate_Synthesis, };
        SimpleResult result = { .cp = 272, .durability = 40, .progress = 548, .quality = 827 };
        SIMPLE_EVAL_AND_CHECK(80, SCRIP_70, recipe, actions, result);
    }
}