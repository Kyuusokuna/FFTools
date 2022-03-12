#pragma once
#include <assert.h>
#include "types.h"
#include "string.h"
#include "Craft_Jobs.h"

#define ALL_ACTIONS ((1 << NUM_ACTIONS) - 1)
#define ALL_ACTIONS_PROGRESS (CF_Muscle_memory | CF_Basic_Synthesis1 | CF_Careful_Synthesis | CF_Prudent_Synthesis | CF_Focused_Synthesis | CF_Delicate_Synthesis | CF_Groundwork)
#define ALL_ACTIONS_QUALITY (CF_Trained_Eye | CF_Reflect | CF_Basic_Touch | CF_Standard_Touch | CF_Advanced_Touch | CF_Prudent_Touch | CF_Focused_Touch | CF_Preparatory_Touch | CF_Byregots_Blessing | CF_Trained_Finesse | CF_Delicate_Synthesis)
#define ALL_ACTIONS_BUFF (CF_Great_Strides | CF_Waste_Not | CF_Waste_Not2 | CF_Veneration | CF_Innovation)
#define ALL_ACTIONS_OTHER (CF_Masters_Mend | CF_Manipulation | CF_Observe)

#define IS_SYNTHESIS_ACTION(action) (CA_Basic_Synthesis <= (action) && (action) <= CA_Delicate_Synthesis)
#define IS_TOUCH_ACTION(action) (CA_Delicate_Synthesis <= (action) && (action) <= CA_Byregots_Blessing)


#ifdef __cplusplus
extern "C" {
#endif

typedef enum : u8 {
    CA_Muscle_memory,
    CA_Reflect,
    CA_Trained_Eye,
    CA_Basic_Synthesis,
    CA_Careful_Synthesis,
    CA_Prudent_Synthesis,
    CA_Focused_Synthesis,
    CA_Groundwork,
    CA_Delicate_Synthesis,
    CA_Basic_Touch,
    CA_Standard_Touch,
    CA_Advanced_Touch,
    CA_Prudent_Touch,
    CA_Focused_Touch,
    CA_Preparatory_Touch,
    CA_Byregots_Blessing,
    CA_Trained_Finesse,
    CA_Observe,
    CA_Masters_Mend,
    CA_Waste_Not,
    CA_Waste_Not2,
    CA_Manipulation,
    CA_Veneration,
    CA_Great_Strides,
    CA_Innovation,

    NUM_ACTIONS,

    NUM_ACTIONS_PROGRESS = 7,
    NUM_ACTIONS_QUALITY = 11,
    NUM_ACTIONS_BUFF = 5,
    NUM_ACTIONS_OTHER = 3,

} Craft_Action;

static_assert(NUM_ACTIONS <= 32, "NUM_ACTIONS needs to be below or equal to 32 so that all actions can fit into a 32-bit integer flag.");

typedef enum : u32 {
    CF_Muscle_memory = (1 << CA_Muscle_memory),
    CF_Reflect = (1 << CA_Reflect),
    CF_Trained_Eye = (1 << CA_Trained_Eye),
    CF_Basic_Synthesis1 = (1 << CA_Basic_Synthesis),
    CF_Careful_Synthesis = (1 << CA_Careful_Synthesis),
    CF_Prudent_Synthesis = (1 << CA_Prudent_Synthesis),
    CF_Focused_Synthesis = (1 << CA_Focused_Synthesis),
    CF_Delicate_Synthesis = (1 << CA_Delicate_Synthesis),
    CF_Groundwork = (1 << CA_Groundwork),
    CF_Basic_Touch = (1 << CA_Basic_Touch),
    CF_Standard_Touch = (1 << CA_Standard_Touch),
    CF_Advanced_Touch = (1 << CA_Advanced_Touch),
    CF_Prudent_Touch = (1 << CA_Prudent_Touch),
    CF_Focused_Touch = (1 << CA_Focused_Touch),
    CF_Preparatory_Touch = (1 << CA_Preparatory_Touch),
    CF_Byregots_Blessing = (1 << CA_Byregots_Blessing),
    CF_Trained_Finesse = (1 << CA_Trained_Finesse),
    CF_Observe = (1 << CA_Observe),
    CF_Masters_Mend = (1 << CA_Masters_Mend),
    CF_Waste_Not = (1 << CA_Waste_Not),
    CF_Waste_Not2 = (1 << CA_Waste_Not2),
    CF_Manipulation = (1 << CA_Manipulation),
    CF_Veneration = (1 << CA_Veneration),
    CF_Great_Strides = (1 << CA_Great_Strides),
    CF_Innovation = (1 << CA_Innovation),
} Craft_Action_Flag;

extern const ROString_Literal Craft_Action_to_string[NUM_ACTIONS];
extern const ROString_Literal Craft_Action_to_Teamcraft_string[NUM_ACTIONS];
extern const ROString_Literal Craft_Action_to_serialization_string[NUM_ACTIONS];

extern const Craft_Action Craft_Actions_Progress[NUM_ACTIONS_PROGRESS];
extern const Craft_Action Craft_Actions_Quality[NUM_ACTIONS_QUALITY];
extern const Craft_Action Craft_Actions_Buff[NUM_ACTIONS_BUFF];
extern const Craft_Action Craft_Actions_Other[NUM_ACTIONS_OTHER];

// (CP, Durability, Progress, Quality)
extern const f32x4 Craft_Actions_efficiency[NUM_ACTIONS];
extern const u8 Craft_Actions_min_lvl[NUM_ACTIONS];

#ifdef __cplusplus
}
#endif