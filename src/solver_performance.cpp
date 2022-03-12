#include <stdio.h>
#include "time.h"
#include "string.h"
#include <ubench/ubench.h>

#include "crafting_solver2.h"
//#include "Craft_Actions.h"

//#define NUM_TEST_SETS 1
//#define ROUNDS_PER_TRY 60000
//#define TRYS_PER_TEST 500

UBENCH_EX(solver_perf, culinarian_80) {
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

UBENCH_MAIN();

/*struct TestResult {
    u64 start_timestamp;
    u64 end_timestamp;

    u64 start_cycle_count;
    u64 end_cycle_count;
};

struct TestDef {
    TestResult results[NUM_TEST_SETS][TRYS_PER_TEST];

    u32 active_actions;

    s32 player_level;
    s32 cp;
    s32 craftsmanship;
    s32 control;

    s32 recipe_level;
    s32 recommended_craftsmanship;
    s32 recommended_control;

    s32 durability;
    s32 progress;
    s32 quality;
};*/


/*
TestDef tests[] = {
    {
        .active_actions = CF_Muscle_memory | CF_Reflect | CF_Name_of_the_Elements | CF_Brand_of_the_Elements | CF_Basic_Synthesis1 | CF_Careful_Synthesis | CF_Focused_Synthesis | CF_Groundwork | CF_Basic_Touch | CF_Standard_Touch | CF_Prudent_Touch | CF_Focused_Touch | CF_Preparatory_Touch | CF_Byregots_Blessing | CF_Masters_Mend | CF_Waste_Not | CF_Waste_Not2 | CF_Inner_Quiet | CF_Veneration | CF_Great_Strides | CF_Innovation,
        
        .player_level = 77,
        .cp = 400,
        .craftsmanship = 1645,
        .control = 1529,

        .recipe_level = 62,
        .recommended_craftsmanship = 1027,
        .recommended_control = 993,

        .durability = 60,
        .progress = 1263,
        .quality = 6864,
    }
};

Crafting_Solver2::Solver_Context init_solver_from_testdef(TestDef def) {
    return Crafting_Solver2::init_solver_context(
        0,
        def.active_actions,

        def.player_level,
        def.cp,
        def.craftsmanship,
        def.control,

        def.recipe_level,
        def.recommended_craftsmanship,
        def.recommended_control,

        def.durability,
        def.progress,
        def.quality
    );
}*/

/*
int main() {
    u32 dummy;
    for (int test = 0; test < sizeof(tests) / sizeof(*tests); test++) {
        auto &def = tests[test];
        auto initial_context = init_solver_from_testdef(def);

        for (int tri = 0; tri < TRYS_PER_TEST; tri++) {
            auto context = initial_context;

            def.results[0][tri].start_timestamp = Time::get_time();
            def.results[0][tri].start_cycle_count = __rdtscp(&dummy);

            for (int round = 0; round < ROUNDS_PER_TRY; round++) {
                Crafting_Solver2::execute_round(context);
            }

            def.results[0][tri].end_cycle_count = __rdtscp(&dummy);
            def.results[0][tri].end_timestamp = Time::get_time();
        }
    }

    for (int i = 0; i < NUM_TEST_SETS; i++) {
        printf("SET: %d\n", i);

        for (int test = 0; test < sizeof(tests) / sizeof(*tests); test++) {
            auto &def = tests[test];

            u64 min_time = UINT64_MAX;
            u64 max_time = 0;
            u64 total_time = 0;

            u64 min_cycles = UINT64_MAX;
            u64 max_cycles = 0;
            u64 total_cycles = 0;

            for (int tri = 0; tri < TRYS_PER_TEST; tri++) {
                auto &result = def.results[i][tri];
                auto tri_time = result.end_timestamp - result.start_timestamp;
                auto tri_cycles = result.end_cycle_count - result.start_cycle_count;

                total_time += tri_time;
                min_time = tri_time < min_time ? tri_time : min_time;
                max_time = tri_time > max_time ? tri_time : max_time;

                total_cycles += tri_cycles;
                min_cycles = tri_cycles < min_cycles ? tri_cycles : min_cycles;
                max_cycles = tri_cycles > max_cycles ? tri_cycles : max_cycles;
            }

            f64 freq = Time::get_frequency() ;
            f64 min = min_time / freq * 1000;
            f64 max = max_time / freq * 1000;
            f64 total = total_time / freq * 1000;
            f64 avg = total / TRYS_PER_TEST;

            f64 min_rps = ROUNDS_PER_TRY / max;
            f64 max_rps = ROUNDS_PER_TRY / min;
            f64 avg_rps = ROUNDS_PER_TRY / avg;

            f64 min_cpr = min_cycles / (f64)ROUNDS_PER_TRY;
            f64 max_cpr = max_cycles / (f64)ROUNDS_PER_TRY;
            f64 avg_cpr = total_cycles / (f64)TRYS_PER_TEST / (f64)ROUNDS_PER_TRY;
            
            printf(" - TEST: %d\n", test);
            printf("  %5s %5s %5s %9s   %9s %9s %9s    %9s %9s %9s\n", "MIN", "AVG", "MAX", "TOTAL", "MIN", "AVG", "MAX", "MIN", "AVG", "MAX");
            printf("  %5.3f %5.3f %5.3f %9.3fms %9.0f %9.0f %9.0frpms %9.0f %9.0f %9.0f\n", min, avg, max, total, min_rps, avg_rps, max_rps, min_cpr, avg_cpr, max_cpr);
        }
    }



    return 0;
}*/