#include "Craft_Actions.h"
#include "string.h"

const constexpr ROString_Literal Craft_Action_to_string[NUM_ACTIONS] = {
    [CA_Muscle_memory] = "Muscle Memory",
    [CA_Reflect] = "Reflect",
    [CA_Trained_Eye] = "Trained Eye",
    [CA_Basic_Synthesis] = "Basic Synthesis",
    [CA_Careful_Synthesis] = "Careful Synthesis",
    [CA_Prudent_Synthesis] = "Prudent Synthesis",
    [CA_Focused_Synthesis] = "Focused Synthesis",
    [CA_Delicate_Synthesis] = "Delicate Synthesis",
    [CA_Groundwork] = "Groundwork",
    [CA_Basic_Touch] = "Basic Touch",
    [CA_Standard_Touch] = "Standard Touch",
    [CA_Advanced_Touch] = "Advanced Touch",
    [CA_Prudent_Touch] = "Prudent Touch",
    [CA_Focused_Touch] = "Focused Touch",
    [CA_Preparatory_Touch] = "Preparatory Touch",
    [CA_Byregots_Blessing] = "Byregot's Blessing",
    [CA_Trained_Finesse] = "Trained Finesse",
    [CA_Observe] = "Observe",
    [CA_Masters_Mend] = "Master's Mend",
    [CA_Waste_Not] = "Waste Not",
    [CA_Waste_Not2] = "Waste Not II",
    [CA_Manipulation] = "Manipulation",
    [CA_Veneration] = "Veneration",
    [CA_Great_Strides] = "Great Strides",
    [CA_Innovation] = "Innovation",
};

const constexpr ROString_Literal Craft_Action_to_Teamcraft_string[NUM_ACTIONS] = {
    [CA_Muscle_memory] = "muscleMemory",
    [CA_Reflect] = "reflect",
    [CA_Trained_Eye] = "trainedEye",
    [CA_Basic_Synthesis] = "basicSynth",
    [CA_Careful_Synthesis] = "carefulSynthesis",
    [CA_Prudent_Synthesis] = "prudentSynthesis", // TODO: verify
    [CA_Focused_Synthesis] = "focusedSynthesis",
    [CA_Delicate_Synthesis] = "delicateSynthesis",
    [CA_Groundwork] = "groundwork",
    [CA_Basic_Touch] = "basicTouch",
    [CA_Standard_Touch] = "standardTouch",
    [CA_Advanced_Touch] = "advancedTouch", // TODO: verify
    [CA_Prudent_Touch] = "prudentTouch",
    [CA_Focused_Touch] = "focusedTouch",
    [CA_Preparatory_Touch] = "preparatoryTouch",
    [CA_Byregots_Blessing] = "byregotsBlessing",
    [CA_Trained_Finesse] = "trainedFinesse", // TODO: verify
    [CA_Observe] = "observe",
    [CA_Masters_Mend] = "mastersMend",
    [CA_Waste_Not] = "wasteNot",
    [CA_Waste_Not2] = "wasteNot2",
    [CA_Manipulation] = "manipulation",
    [CA_Veneration] = "veneration",
    [CA_Great_Strides] = "greatStrides",
    [CA_Innovation] = "innovation",
};

const constexpr ROString_Literal Craft_Action_to_serialization_string[NUM_ACTIONS] = {
    [CA_Muscle_memory] = "muscle_memory",
    [CA_Reflect] = "reflect",
    [CA_Trained_Eye] = "trained_eye",
    [CA_Basic_Synthesis] = "basic_synthesis",
    [CA_Careful_Synthesis] = "careful_synthesis",
    [CA_Prudent_Synthesis] = "prudent_synthesis",
    [CA_Focused_Synthesis] = "focused_synthesis",
    [CA_Delicate_Synthesis] = "delicate_synthesis",
    [CA_Groundwork] = "groundwork",
    [CA_Basic_Touch] = "basic_touch",
    [CA_Standard_Touch] = "standard_touch",
    [CA_Advanced_Touch] = "advanced_touch",
    [CA_Prudent_Touch] = "prudent_touch",
    [CA_Focused_Touch] = "focused_touch",
    [CA_Preparatory_Touch] = "preparatory_touch",
    [CA_Byregots_Blessing] = "byregots_blessing",
    [CA_Trained_Finesse] = "trained_finesse",
    [CA_Observe] = "observe",
    [CA_Masters_Mend] = "masters_mend",
    [CA_Waste_Not] = "waste_not",
    [CA_Waste_Not2] = "waste_not2",
    [CA_Manipulation] = "manipulation",
    [CA_Veneration] = "veneration",
    [CA_Great_Strides] = "great_strides",
    [CA_Innovation] = "innovation",
};

const Craft_Action Craft_Actions_Progress[NUM_ACTIONS_PROGRESS] = {
    CA_Muscle_memory,
    CA_Basic_Synthesis,
    CA_Careful_Synthesis,
    CA_Prudent_Synthesis,
    CA_Focused_Synthesis,
    CA_Delicate_Synthesis,
    CA_Groundwork,
};

const Craft_Action Craft_Actions_Quality[NUM_ACTIONS_QUALITY] = {
    CA_Trained_Eye,
    CA_Reflect,
    CA_Basic_Touch,
    CA_Standard_Touch,
    CA_Advanced_Touch,
    CA_Prudent_Touch,
    CA_Focused_Touch,
    CA_Preparatory_Touch,
    CA_Byregots_Blessing,
    CA_Trained_Finesse,
    CA_Delicate_Synthesis,
};

const Craft_Action Craft_Actions_Buff[NUM_ACTIONS_BUFF] = {
    CA_Great_Strides,
    CA_Waste_Not,
    CA_Waste_Not2,
    CA_Veneration,
    CA_Innovation,
};

const Craft_Action Craft_Actions_Other[NUM_ACTIONS_OTHER] = {
    CA_Masters_Mend,
    CA_Manipulation,
    CA_Observe,
};

// (CP, Durability, Progress, Quality)
const constexpr f32x4 Craft_Actions_efficiency[NUM_ACTIONS] = {
    [CA_Muscle_memory] = {6.0, 10.0, 3.0, 0.0},
    [CA_Reflect] = {6.0, 10.0, 0.0, 1.0},
    [CA_Trained_Eye] = {250.0, 0.0, 0.0, 0.0},

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

    [CA_Masters_Mend] = {88.0, 0.0, 0.0, 0.0},
    [CA_Waste_Not] = {56.0, 0.0, 0.0, 0.0},
    [CA_Waste_Not2] = {98.0, 0.0, 0.0, 0.0},
    [CA_Manipulation] = {96.0, 0.0, 0.0, 0.0},

    [CA_Veneration] = {18.0, 0.0, 0.0, 0.0},
    [CA_Great_Strides] = {32.0, 0.0, 0.0, 0.0},
    [CA_Innovation] = {18.0, 0.0, 0.0, 0.0},
};

const constexpr uint8_t Craft_Actions_min_lvl[NUM_ACTIONS] = {
    [CA_Muscle_memory] = 54,
    [CA_Reflect] = 69,
    [CA_Trained_Eye] = 80,
    [CA_Basic_Synthesis] = 1,
    [CA_Careful_Synthesis] = 62,
    [CA_Prudent_Synthesis] = 88,
    [CA_Focused_Synthesis] = 67,
    [CA_Delicate_Synthesis] = 76,
    [CA_Groundwork] = 72,
    [CA_Basic_Touch] = 5,
    [CA_Standard_Touch] = 18,
    [CA_Advanced_Touch] = 84,
    [CA_Prudent_Touch] = 66,
    [CA_Focused_Touch] = 68,
    [CA_Preparatory_Touch] = 71,
    [CA_Byregots_Blessing] = 50,
    [CA_Trained_Finesse] = 90,
    [CA_Observe] = 13,
    [CA_Masters_Mend] = 7,
    [CA_Waste_Not] = 15,
    [CA_Waste_Not2] = 47,
    [CA_Manipulation] = 65,
    [CA_Veneration] = 15,
    [CA_Great_Strides] = 21,
    [CA_Innovation] = 26,
}; 

static_assert([]() constexpr -> bool { for (auto e : Craft_Action_to_string) if (!e.length) return false; return true; }(), "Not all Craft_Actions have a user readable string defined.");
static_assert([]() constexpr -> bool { for (auto e : Craft_Action_to_Teamcraft_string) if (!e.length) return false; return true; }(), "Not all Craft_Actions have a Teamcraft string defined.");
static_assert([]() constexpr -> bool { for (auto e : Craft_Action_to_serialization_string) if (!e.length) return false; return true; }(), "Not all Craft_Actions have a serialization string defined.");
static_assert([]() constexpr -> bool { for (auto e : Craft_Actions_min_lvl) if (!e) return false; return true; }(), "Not all Craft_Actions have a minimum level defined.");

static_assert([]() constexpr -> bool { for (auto e : Craft_Actions_efficiency) if (!e.x && !e.y && !e.z && !e.w) return false; return true; }(), "Not all Craft_Actions have their efficiency set (CP, Durability, Progress Quality).");