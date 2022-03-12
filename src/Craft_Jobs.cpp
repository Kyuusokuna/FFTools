#include "Craft_Jobs.h"

const constexpr ROString_Literal Craft_Job_to_string[NUM_JOBS] = {
    [Craft_Job_CRP] = "Carpenter",
    [Craft_Job_BSM] = "Blacksmith",
    [Craft_Job_ARM] = "Armorer",
    [Craft_Job_GSM] = "Goldsmith",
    [Craft_Job_LTW] = "Leatherworker",
    [Craft_Job_WVR] = "Weaver",
    [Craft_Job_ALC] = "Alchemist",
    [Craft_Job_CUL] = "Culinarian",
};

static_assert([]() constexpr -> bool { for (auto e : Craft_Job_to_string) if (!e.length) return false; return true; }(), "Not all Craft_Jobs have a user readable string defined.");