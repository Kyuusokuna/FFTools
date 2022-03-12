#pragma once
#include "types.h"
#include "string.h"

typedef enum : u8 {
    Craft_Job_CRP = 0,
    Craft_Job_BSM = 1,
    Craft_Job_ARM = 2,
    Craft_Job_GSM = 3,
    Craft_Job_LTW = 4,
    Craft_Job_WVR = 5,
    Craft_Job_ALC = 6,
    Craft_Job_CUL = 7,

    NUM_JOBS = 8,
} Craft_Job;


extern const ROString_Literal Craft_Job_to_string[NUM_JOBS];