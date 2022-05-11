#include "crafting_simulator.h"

namespace Crafting_Simulator {
    s32 calc_progress_increase(s32 player_level, s32 recipe_level, s32 craftsmanship, s32 progress_divider, s32 progress_modifier) {
        s32 modifier = player_level <= recipe_level ? progress_modifier : 100;
        s32 increase = (craftsmanship * 10 + progress_divider * 2) * modifier / (100 * progress_divider);

        return increase;
    }

    s32 calc_quality_increase(s32 player_level, s32 recipe_level, s32 control, s32 quality_divider, s32 quality_modifier) {
        s32 modifier = player_level <= recipe_level ? quality_modifier : 100;
        s32 increase = (control * 10 + quality_divider * 35) * modifier / (100 * quality_divider);

        return increase;
    }

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
                         s32 quality) {
        Context context;

        context.player_level = player_level;

        context.initial_state.x = cp;
        context.initial_state.y = durability;
        context.initial_state.z = progress;
        context.initial_state.w = quality;

        context.base_change = f32x4(1, 1, calc_progress_increase(player_level, recipe_level, craftsmanship, progress_divider, progress_modifier), 
                                          calc_quality_increase(player_level, recipe_level, control, quality_divider, quality_modifier));

        context.turns = s16x8(0, 0, 0, 0, 0, 0, 0, 0);

        context.state = context.initial_state;
        context.previous_action = NUM_ACTIONS;

        context.step = 0;

        return context;
    }

    __m128i select_epi8(__m128i a, __m128i b, __m128i selector) {
        return _mm_or_si128(_mm_and_si128(selector, b), _mm_andnot_si128(selector, a));
    }

    __m128i min_epi32(__m128i a, __m128i b) {
        auto cmp = _mm_cmpgt_epi32(a, b);
        return select_epi8(a, b, cmp);
    }

    __m128i max_epi32(__m128i a, __m128i b) {
        auto cmp = _mm_cmplt_epi32(a, b);
        return select_epi8(a, b, cmp);
    }

    __m128i clamp_epi32(__m128i x, __m128i min, __m128i max) {
        return min_epi32(max_epi32(x, min), max);
    }

    #define muscle_memory_turns (context.turns.e0.x)
    #define great_strides_turns (context.turns.e0.y)
    #define veneration_turns (context.turns.e0.z)
    #define innovation_turns (context.turns.e0.w)

    #define manipulation_turns (context.turns.e1.x)
    #define waste_not_turns (context.turns.e1.y)
    #define name_of_the_elements_turns (context.turns.e1.z)

    #define inner_quiet_stacks (context.turns.e1.w)
    #define inner_quiet_active (context.player_level >= 11)

    #define basic_synthesis_mastery (context.player_level >= 31)
    #define careful_synthesis_mastery (context.player_level >= 82)
    #define groundwork_mastery (context.player_level >= 86)

    // (CP, Durability, Progress, Quality)
    constexpr f32x4 efficiency[NUM_ACTIONS] = {
        [CA_Muscle_memory] = {6.0, 10.0, 3.0, 0.0},
        [CA_Reflect] = {6.0, 10.0, 0.0, 1.0},
        [CA_Trained_Eye] = {250.0, 10.0, 0.0, 999999.0},

        [CA_Basic_Synthesis] = {0.0, 10.0, 1.0, 0.0},
        [CA_Careful_Synthesis] = {7.0, 10.0, 1.5, 0.0},
        [CA_Prudent_Synthesis] = {18.0, 5.0, 1.8, 0.0},
        [CA_Focused_Synthesis] = {5.0, 10.0, 2.0, 0.0},
        [CA_Delicate_Synthesis] = {32.0, 10.0, 1.0, 1.0},

        [CA_Groundwork] = {18.0, 20.0, 3.0, 0.0},

        [CA_Basic_Touch] = {18.0, 10.0, 0.0, 1.0},
        [CA_Standard_Touch] = {32.0, 10.0, 0.0, 1.25},
        [CA_Advanced_Touch] = {46.0, 10.0, 0.0, 1.5},
        [CA_Prudent_Touch] = {25.0, 5.0, 0.0, 1.0},
        [CA_Focused_Touch] = {18.0, 10.0, 0.0, 1.5},
        [CA_Preparatory_Touch] = {40.0, 20.0, 0.0, 2.0},

        [CA_Byregots_Blessing] = {24.0, 10.0, 0.0, 1.0},
        [CA_Trained_Finesse] = {32.0, 0.0, 0.0, 1.0},

        [CA_Observe] = {7.0, 0.0, 0.0, 0.0},

        [CA_Masters_Mend] = {88.0, -30.0, 0.0, 0.0},
        [CA_Waste_Not] = {56.0, 0.0, 0.0, 0.0},
        [CA_Waste_Not2] = {98.0, 0.0, 0.0, 0.0},
        [CA_Manipulation] = {96.0, 0.0, 0.0, 0.0},

        [CA_Veneration] = {18.0, 0.0, 0.0, 0.0},
        [CA_Great_Strides] = {32.0, 0.0, 0.0, 0.0},
        [CA_Innovation] = {18.0, 0.0, 0.0, 0.0},
    };

    template<Craft_Action action>
    bool simulate_step_internal(Context &context) {
        if (_mm_movemask_epi8(_mm_cmpeq_epi32(context.state.m128i, _mm_setzero_si128())) & 0xFF0)
            return false;

        if ((action <= CA_Trained_Eye) && context.step != 0)
            return false;

        if (action == CA_Prudent_Touch && waste_not_turns)
            return false;

        if (action == CA_Prudent_Synthesis && waste_not_turns)
            return false;

        if (action == CA_Byregots_Blessing && !inner_quiet_stacks)
            return false;

        if (action == CA_Focused_Touch && context.previous_action != CA_Observe)
            return false;

        if (action == CA_Focused_Synthesis && context.previous_action != CA_Observe)
            return false;

        if (action == CA_Trained_Finesse && inner_quiet_stacks != 10)
            return false;

        f32x4 base_action_change = efficiency[action];

        if (action == CA_Basic_Synthesis && basic_synthesis_mastery)
            base_action_change.z = 1.2f;

        if (action == CA_Careful_Synthesis && careful_synthesis_mastery)
            base_action_change.z = 1.8f;

        if (action == CA_Groundwork && groundwork_mastery)
            base_action_change.z = 3.6f;

        if (IS_TOUCH_ACTION(action))
            base_action_change.w += base_action_change.w * inner_quiet_stacks * 0.1f;

        f32x4 action_change = base_action_change;

        // CP Modifiers
        if (action == CA_Standard_Touch && context.previous_action == CA_Basic_Touch)
            action_change.x = 18;

        if (action == CA_Advanced_Touch && context.previous_action == CA_Standard_Touch)
            action_change.x = 18;


        // Durability Modifiers
        if (waste_not_turns)
            action_change.y *= .5;


        // Progress Modifiers
        if (IS_SYNTHESIS_ACTION(action)) {
            if(muscle_memory_turns)
                action_change.z += base_action_change.z;

            if (veneration_turns)
                action_change.z += base_action_change.z * 0.5f;
        }


        // Quality Modifiers
        if (IS_TOUCH_ACTION(action)) {
            if (great_strides_turns)
                action_change.w += base_action_change.w;

            if (innovation_turns)
                action_change.w += base_action_change.w * 0.5f;
        }

        if (action == CA_Byregots_Blessing)
            action_change.w += action_change.w * inner_quiet_stacks * 0.2f;

        // Other Modifiers
        if (action == CA_Groundwork && context.state.y < action_change.y)
            action_change.z *= 0.5;

        s32x4 new_state = _mm_sub_epi32(context.state.m128i, _mm_cvttps_epi32(_mm_mul_ps(context.base_change.m128, action_change.m128)));
        new_state.m128i = clamp_epi32(new_state.m128i, _mm_setr_epi32(INT_MIN, INT_MIN, 0, 0), _mm_setr_epi32(INT_MAX, context.initial_state.y, INT_MAX, INT_MAX));

        // no CP left
        if (new_state.x < 0)
            return false;

        // no durability left and not finished
        if (new_state.y <= 0 && new_state.z)
            return false;

        if (action == CA_Trained_Eye)
            new_state.w = 0;

        if (manipulation_turns)
            new_state.y = clamp(new_state.y + 5, INT_MIN, context.initial_state.y);

        context.state = new_state;
        _mm_storeu_si128(&context.turns.m128i, _mm_subs_epu16(_mm_loadu_si128(&context.turns.m128i), _mm_setr_epi16(1, 1, 1, 1, 1, 1, 0, 0)));

        if (action == CA_Muscle_memory)
            muscle_memory_turns = 5;

        if (action == CA_Great_Strides)
            great_strides_turns = 3;

        if (action == CA_Veneration)
            veneration_turns = 4;

        if (action == CA_Innovation)
            innovation_turns = 4;

        if (action == CA_Manipulation)
            manipulation_turns = 8;

        if (action == CA_Waste_Not)
            waste_not_turns = 4;

        if (action == CA_Waste_Not2)
            waste_not_turns = 8;

        if (action == CA_Reflect)
            inner_quiet_stacks = 2;


        if (IS_TOUCH_ACTION(action)) {
            great_strides_turns = 0;

            if (inner_quiet_active) {
                inner_quiet_stacks++;

                if(action == CA_Preparatory_Touch)
                    inner_quiet_stacks++;

                inner_quiet_stacks = inner_quiet_stacks > 10 ? 10 : inner_quiet_stacks;
            }
        }

        if (action == CA_Byregots_Blessing)
            inner_quiet_stacks = 0;


        if (IS_SYNTHESIS_ACTION(action))
            muscle_memory_turns = 0;

        context.previous_action = action;
        context.step++;

        return true;

        /*
        if (context.manipulation_turns) // @TODO is this the right place for manipulation?? TEST THIS
            context.state.y += 5;


        return true;*/
    }

    typedef bool simulate_function_signature(Context &context);

    constexpr simulate_function_signature *simulate_funcs[NUM_ACTIONS] = {
        [CA_Muscle_memory] = simulate_step_internal<CA_Muscle_memory>,
        [CA_Reflect] = simulate_step_internal<CA_Reflect>,
        [CA_Trained_Eye] = simulate_step_internal<CA_Trained_Eye>,
        [CA_Basic_Synthesis] = simulate_step_internal<CA_Basic_Synthesis>,
        [CA_Careful_Synthesis] = simulate_step_internal<CA_Careful_Synthesis>,
        [CA_Prudent_Synthesis] = simulate_step_internal<CA_Prudent_Synthesis>,
        [CA_Focused_Synthesis] = simulate_step_internal<CA_Focused_Synthesis>,
        [CA_Groundwork] = simulate_step_internal<CA_Groundwork>,
        [CA_Delicate_Synthesis] = simulate_step_internal<CA_Delicate_Synthesis>,
        [CA_Basic_Touch] = simulate_step_internal<CA_Basic_Touch>,
        [CA_Standard_Touch] = simulate_step_internal<CA_Standard_Touch>,
        [CA_Advanced_Touch] = simulate_step_internal<CA_Advanced_Touch>,
        [CA_Prudent_Touch] = simulate_step_internal<CA_Prudent_Touch>,
        [CA_Focused_Touch] = simulate_step_internal<CA_Focused_Touch>,
        [CA_Preparatory_Touch] = simulate_step_internal<CA_Preparatory_Touch>,
        [CA_Byregots_Blessing] = simulate_step_internal<CA_Byregots_Blessing>,
        [CA_Trained_Finesse] = simulate_step_internal<CA_Trained_Finesse>,
        [CA_Observe] = simulate_step_internal<CA_Observe>,
        [CA_Masters_Mend] = simulate_step_internal<CA_Masters_Mend>,
        [CA_Waste_Not] = simulate_step_internal<CA_Waste_Not>,
        [CA_Waste_Not2] = simulate_step_internal<CA_Waste_Not2>,
        [CA_Manipulation] = simulate_step_internal<CA_Manipulation>,
        [CA_Veneration] = simulate_step_internal<CA_Veneration>,
        [CA_Great_Strides] = simulate_step_internal<CA_Great_Strides>,
        [CA_Innovation] = simulate_step_internal<CA_Innovation>,
    };
    static_assert([]() constexpr -> bool { for (auto e : simulate_funcs) if (!e) return false; return true; }(), "Not all Craft_Actions have have a simulate function.");


    bool simulate_step(Context &context, Craft_Action action) {
            return simulate_funcs[action](context);
    }
}