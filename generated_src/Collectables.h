#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_COLLECTABILITY_INFOS (201)

struct Collectability_Info {
    uint16_t low;
    uint16_t medium;
    uint16_t high;
};

extern const Collectability_Info Collectability_Infos[NUM_COLLECTABILITY_INFOS];

#ifdef __cplusplus
}
#endif
