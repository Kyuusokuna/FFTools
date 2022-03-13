#include "crafting_simulator.h"
#include <superluminal/PerformanceAPI.h>

// KNOWN ISSUES:
//    CA_Manipulation,

namespace Crafting_Simulator {
    s32 calc_progress_increase(s32 player_level, s32 recipe_level, s32 craftsmanship, s32 progress_divider, s32 progress_modifier) {
        s32 increase = craftsmanship * 10 / progress_divider + 2;
        if (player_level < recipe_level)
            increase = increase * progress_modifier / 100;

        return increase;
    }

    s32 calc_quality_increase(s32 player_level, s32 recipe_level, s32 control, s32 quality_divider, s32 quality_modifier) {
        s32 increase = control * 10 / quality_divider + 35;
        if (player_level < recipe_level)
            increase = increase * quality_modifier / 100;

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

        context.initial_state.x = cp;
        context.initial_state.y = durability;
        context.initial_state.z = progress;
        context.initial_state.w = quality;

        context.craftsmanship = craftsmanship;
        context.control = control;

        context.base_change = f32x4(1, 1, calc_progress_increase(player_level, recipe_level, craftsmanship, progress_divider, progress_modifier), 
                                          calc_quality_increase(player_level, recipe_level, control, quality_divider, quality_modifier));

        context.inner_quiet_active = player_level >= 11;
        context.basic_synthesis_mastery = player_level >= 31;
        context.careful_synthesis_mastery = player_level >= 82;
        context.groundwork_mastery = player_level >= 86;

        context.turns = s16x8(0, 0, 0, 0, 0, 0, 0, 0);

        context.state = context.initial_state;
        context.previous_action = NUM_ACTIONS;

        context.step = 0;

        return context;
    }

    #define muscle_memory_turns turns.e0.x
    #define great_strides_turns turns.e0.y
    #define veneration_turns turns.e0.z
    #define innovation_turns turns.e0.w

    #define manipulation_turns turns.e1.x
    #define waste_not_turns turns.e1.y
    #define name_of_the_elements_turns turns.e1.z

    #define inner_quiet_stacks turns.e1.w

    #include <intrin.h>
    int cpuid2() {
        int info[4];
        __cpuid(info, 1);
        return info[2];
    }

    __m128i select_epi8(__m128i a, __m128i b, __m128i selector) {
        return _mm_or_si128(_mm_and_si128(selector, b), _mm_andnot_si128(selector, a));
    }

    __attribute__((target("sse4.1")))
    __m128i select_epi8_sse4_1(__m128i a, __m128i b, __m128i selector) {
        return _mm_blendv_epi8(a, b, selector);
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

    __attribute__((target("sse4.1")))
    __m128i clamp_epi32_sse4_1(__m128i x, __m128i min, __m128i max) {
        return _mm_min_epi32(_mm_max_epi32(x, min), max);
    }
    
    // KNOWN ISSUES:
    //    CA_Manipulation,

    bool simulate_step(Context &context, Craft_Action action) {
        if (_mm_movemask_epi8(_mm_cmpeq_epi32(context.state.m128i, _mm_setzero_si128())) & 0xFF0)
            return false;

        if (context.step != 0 && (action <= CA_Trained_Eye))
            return false;

        if (action == CA_Prudent_Touch && context.waste_not_turns)
            return false;

        if (action == CA_Prudent_Synthesis && context.waste_not_turns)
            return false;

        if (action == CA_Byregots_Blessing && !context.inner_quiet_stacks)
            return false;

        if (action == CA_Focused_Touch && context.previous_action != CA_Observe)
            return false;

        if (action == CA_Focused_Synthesis && context.previous_action != CA_Observe)
            return false;

        if (action == CA_Trained_Finesse && context.inner_quiet_stacks != 10)
            return false;

        bool is_touch_action = IS_TOUCH_ACTION(action);
        bool is_synthesis_action = IS_SYNTHESIS_ACTION(action);

        f32x4 base_action_change = Craft_Actions_efficiency[action];

        if (context.basic_synthesis_mastery && action == CA_Basic_Synthesis)
            base_action_change.z = 1.2f;

        if (context.careful_synthesis_mastery && action == CA_Careful_Synthesis)
            base_action_change.z = 1.8f;

        if (context.groundwork_mastery && action == CA_Groundwork)
            base_action_change.z = 3.6f;

        if (is_touch_action)
            base_action_change.w += base_action_change.w * context.inner_quiet_stacks * 0.1f;

        f32x4 action_change = base_action_change;

        // CP Modifiers
        if (action == CA_Standard_Touch && context.previous_action == CA_Basic_Touch)
            action_change.x = 18;

        if (action == CA_Advanced_Touch && context.previous_action == CA_Standard_Touch)
            action_change.x = 18;


        // Durability Modifiers
        if (context.waste_not_turns)
            action_change.y *= .5;


        // Progress Modifiers
        if (context.muscle_memory_turns && is_synthesis_action)
            action_change.z += base_action_change.z;

        if (context.veneration_turns && is_synthesis_action)
            action_change.z += base_action_change.z * 0.5f;


        // Quality Modifiers
        if (context.great_strides_turns && is_touch_action)
            action_change.w += base_action_change.w;

        if (context.innovation_turns && is_touch_action)
            action_change.w += base_action_change.w * 0.5f;

        if (action == CA_Byregots_Blessing)
            action_change.w += action_change.w * context.inner_quiet_stacks * 0.2f;

        // Other Modifiers
        if (action == CA_Groundwork && context.state.y < action_change.y)
            action_change.z *= 0.5;

        if (action == CA_Masters_Mend)
            action_change.y = -30;

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

        context.state = new_state;

        __m128i new_turns = _mm_subs_epu16(context.turns.m128i, _mm_setr_epi16(1, 1, 1, 1, 1, 1, 0, 0));

        __m128i turns_actions = _mm_setr_epi16(CA_Muscle_memory, CA_Great_Strides, CA_Veneration, CA_Innovation, CA_Manipulation, CA_Waste_Not, NUM_ACTIONS, CA_Reflect);
        __m128i turns_new_values = _mm_setr_epi16(5, 3, 4, 4, 8, 4, 0, 2);

        __m128i actionx8 = _mm_set1_epi16(action);
        __m128i turns_write_mask = _mm_cmpeq_epi16(actionx8, turns_actions);
        context.turns.m128i = select_epi8(new_turns, turns_new_values, turns_write_mask);

        if (context.inner_quiet_active && is_touch_action)
            context.inner_quiet_stacks++;

        if (context.inner_quiet_active && action == CA_Preparatory_Touch)
            context.inner_quiet_stacks++;

        if (context.great_strides_turns && is_touch_action)
            context.great_strides_turns = 0;

        if (context.muscle_memory_turns && is_synthesis_action)
            context.muscle_memory_turns = 0;

        if (action == CA_Byregots_Blessing)
            context.inner_quiet_stacks = 0;

        if (action == CA_Waste_Not2)
            context.waste_not_turns = 8;

        context.inner_quiet_stacks = context.inner_quiet_stacks > 10 ? 10 : context.inner_quiet_stacks;


        context.previous_action = action;
        context.step++;

        return true;

        /*
        if (action == CA_Byregots_Blessing) {
            if (!context.inner_quiet_stacks)
                return false;

            action_change.w += context.inner_quiet_stacks * 0.2;
        }

        if (context.manipulation_turns) // @TODO is this the right place for manipulation?? TEST THIS
            context.state.y += 5;
        

        return true;*/
    }

}