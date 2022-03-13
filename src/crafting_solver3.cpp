#include "crafting_solver3.h"
#include <assert.h>
#include <string.h>
#include <superluminal/PerformanceAPI.h>
#include "time.h"

namespace Crafting_Solver3 {
    /*inline u32 pcg32_output(u32 &state) {
        u32 word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
        return (word >> 22u) ^ word;
    }

    inline u32 pcg32_rand(u32 &state) {
        u32 old_state = state;
        state = state * 747796405U + 2891336453U;
        return pcg32_output(old_state);
    }*/

    //#define pcg_rotr_32(value, rot) ((value >> rot) | (value << ((- rot) & 31)))

    inline uint32_t pcg_rotr_32(uint32_t value, unsigned int rot) {
        asm("rorl   %%cl, %0" : "=r" (value) : "0" (value), "c" (rot));
        return value;
    }

    inline u32 pcg32_output(u64 &state) {
        return pcg_rotr_32(((state >> 18u) ^ state) >> 27u, state >> 59u);
    }

    inline u32 pcg32_rand(u64 &state) {
        u64 old_state = state;
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return pcg32_output(old_state);
    }

    u32 nth_bit(u32 v, u32 r) {
        r++;

        u32 a = v - ((v >> 1) & 0x55555555);
        u32 b = ((a >> 2) & 0x33333333) + (a & 0x33333333);
        u32 c = ((b >> 4) + b) & 0x0F0F0F0F;
        u32 d = ((c >> 8) + c) & 0x00FF00FF;
        u32 t = (d >> 16) + (d >> 24);

        u32 s = 32;
        s -= ((t - r) & 256) >> 4; r -= (t & ((t - r) >> 8));
        t = (c >> (s - 8)) & 0xf;
        s -= ((t - r) & 256) >> 5; r -= (t & ((t - r) >> 8));
        t = (b >> (s - 4)) & 0x7;
        s -= ((t - r) & 256) >> 6; r -= (t & ((t - r) >> 8));
        t = (a >> (s - 2)) & 0x3;
        s -= ((t - r) & 256) >> 7; r -= (t & ((t - r) >> 8));
        t = (v >> (s - 1)) & 0x1;
        s -= ((t - r) & 256) >> 8;

        return s - 1;
    }

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
        s32 quality) {
        Solver_Context context = {};

        context.seed = (0x912B8F17ULL << 32) + seed;

        context.current_best.seed = 0;
        context.current_best.state = s32x4(cp, durability, progress, quality);
        context.current_best.depth = 0;

        for (int i = 0; i < MAX_SOLVE_LENGTH; i++)
            context.current_best.actions[i] = (Craft_Action)-1;

        if (active_actions & CF_Focused_Touch && !(active_actions & CF_Observe))
            active_actions &= ~CF_Focused_Touch;

        if (active_actions & CF_Focused_Synthesis && !(active_actions & CF_Observe))
            active_actions &= ~CF_Focused_Synthesis;

        if (active_actions & CF_Observe && !(active_actions & (CF_Focused_Touch | CF_Focused_Synthesis)))
            active_actions &= ~CF_Observe;

        if (active_actions & CF_Trained_Eye && ((player_level - 10) < recipe_level))
            active_actions &= ~CF_Trained_Eye;

        context.active_actions = active_actions;

        if (active_actions)
            for (int i = 0; i < 32; i++)
                context.active_actions_lookup_table[i] = (Craft_Action)nth_bit(active_actions, i % __builtin_popcount(active_actions));


        context.sim_context = Crafting_Simulator::init_context(player_level,
                                                               cp,
                                                               craftsmanship,
                                                               control,

                                                               recipe_level,
                                                               progress_divider,
                                                               progress_modifier,
                                                               quality_divider,
                                                               quality_modifier,

                                                               durability,
                                                               progress,
                                                               quality);

        return context;
    }

    bool b_better_than_a(Result a, Result b) {
        if (b.state.z < a.state.z || // higher Progress
            (b.state.z == a.state.z && b.state.w < a.state.w) || // same   Progress, higher Quality
            (b.state.z == a.state.z && b.state.w == a.state.w && b.depth < a.depth) || // same Progress, same Quality, less turns
            (b.state.z == a.state.z && b.state.w == a.state.w && b.depth == a.depth && b.state.x > a.state.x)) { // same Progress, same Quality, same turns, less cp used
            return true;
        }

        return false;
    }

    void execute_round(Solver_Context &context) {
        PERFORMANCEAPI_INSTRUMENT_FUNCTION();

        if (!context.active_actions)
            return;

        Craft_Action actions[MAX_SOLVE_LENGTH];

        int current_active_actions = context.active_actions;
        //int previous_action = 0;
        Crafting_Simulator::Context sim_context = context.sim_context;


        for (int i = 0; i < MAX_SOLVE_LENGTH; i++) {
            auto rand_val = pcg32_rand(context.seed);
            Craft_Action action = context.active_actions_lookup_table[rand_val & 0x1F];

            if ((current_active_actions & CF_Standard_Touch) && action == CA_Basic_Touch && sim_context.previous_action == CA_Basic_Touch)
                action = (Craft_Action)CA_Standard_Touch;

            if ((current_active_actions & CF_Advanced_Touch) && (action == CA_Standard_Touch || action == CA_Basic_Touch) && sim_context.previous_action == CA_Standard_Touch)
                action = (Craft_Action)CA_Advanced_Touch;


            actions[i] = (Craft_Action)action;

            current_active_actions &= ~(CF_Muscle_memory | CF_Reflect | CF_Trained_Eye);

            if (!simulate_step(sim_context, (Craft_Action)action))
                return;

            Result new_result;
            new_result.state = sim_context.state;
            new_result.depth = i + 1;

            if (b_better_than_a(context.current_best, new_result)) {
                for (int i = 0; i < MAX_SOLVE_LENGTH; i++)
                    new_result.actions[i] = actions[i];
                context.current_best = new_result;
            }
        }
    }
}