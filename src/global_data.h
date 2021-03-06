#pragma once
#include <stdatomic.h>
#include <d3d11.h>
#include "Array.h"
#include "types.h"
#include "Craft_Actions.h"
#include "../generated_src/Recipes.h"
#include <imgui/imgui.h>
#include "crafting_solver3.h"

#define MAX_SIM_ACTIONS 100

struct Item_List {
    struct Entry {
        Item item;
        u32 amount;
    };

    struct Selected_Recipe {
        Item item;
        Recipe *selected_recipe;

        u32 num_possible_recipes;
        Recipe *possible_recipes[NUM_JOBS];
    };

    char name[64];

    Array<Entry> items = {};
    Array<Selected_Recipe> selected_recipes = {};
};

enum Main_Panel {
    Main_Panel_Profile,
    Main_Panel_Crafting_Simulator,
    Main_Panel_Crafting_Solver,
    Main_Panel_Lists,
};

extern struct Global_Data {
    Main_Panel current_main_panel = Main_Panel_Profile;

    struct Craft_Setup {
        Craft_Job selected_job = (Craft_Job)-1;

        struct Profile {
            u32 active_actions = 0;
            s32 level = 1;
            s32 craftsmanship = 0;
            s32 control = 0;
            s32 cp = 180;
            bool specialist = false;
        } job_settings[NUM_JOBS];
    } craft_setup;

    struct {
        Craft_Job selected_job = (Craft_Job)-1;
        struct {
            Recipe *selected_recipe = 0;
            s16 num_actions = 0;
            Craft_Action actions[MAX_SIM_ACTIONS] = {};
        } per_job[NUM_JOBS];
    } simulator;

    struct {
        Craft_Job selected_job = (Craft_Job)-1;

        struct {
            Recipe *selected_recipe = 0;
            Crafting_Solver3::Result current_best;
        } per_job[NUM_JOBS];

        bool running_sim = false;
    } solver;

    struct {
        Array<Item_List> item_lists;
        Item_List *selected_list;
    } lists;

    int32_t lines_per_macro = 13;

    ID3D11ShaderResourceView *actions_texture;

    ImFont *big_font;

    f32 button_size;
    f32 input_int_width;
    f32 input_str_width;
} global_data;