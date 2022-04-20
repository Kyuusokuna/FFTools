#include "Collectables.h"
#include <miniz/miniz.h>

#ifdef __cplusplus
extern "C" {
#endif

const Collectability_Info Collectability_Infos[NUM_COLLECTABILITY_INFOS] {
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 110, .medium = 160, .high = 110 },
    { .low = 160, .medium = 220, .high = 160 },
    { .low = 49, .medium = 81, .high = 49 },
    { .low = 141, .medium = 236, .high = 141 },
    { .low = 209, .medium = 349, .high = 209 },
    { .low = 294, .medium = 492, .high = 294 },
    { .low = 74, .medium = 123, .high = 74 },
    { .low = 72, .medium = 120, .high = 72 },
    { .low = 311, .medium = 520, .high = 311 },
    { .low = 118, .medium = 197, .high = 118 },
    { .low = 171, .medium = 286, .high = 171 },
    { .low = 376, .medium = 629, .high = 376 },
    { .low = 19, .medium = 54, .high = 19 },
    { .low = 1039, .medium = 1740, .high = 1039 },
    { .low = 622, .medium = 761, .high = 622 },
    { .low = 650, .medium = 1088, .high = 650 },
    { .low = 393, .medium = 658, .high = 393 },
    { .low = 508, .medium = 850, .high = 508 },
    { .low = 19, .medium = 54, .high = 19 },
    { .low = 557, .medium = 932, .high = 557 },
    { .low = 346, .medium = 579, .high = 346 },
    { .low = 137, .medium = 228, .high = 137 },
    { .low = 314, .medium = 526, .high = 314 },
    { .low = 53, .medium = 88, .high = 53 },
    { .low = 206, .medium = 344, .high = 206 },
    { .low = 5, .medium = 8, .high = 5 },
    { .low = 23, .medium = 38, .high = 23 },
    { .low = 7, .medium = 11, .high = 7 },
    { .low = 470, .medium = 787, .high = 470 },
    { .low = 23, .medium = 38, .high = 23 },
    { .low = 19, .medium = 32, .high = 19 },
    { .low = 785, .medium = 1315, .high = 785 },
    { .low = 314, .medium = 526, .high = 314 },
    { .low = 17, .medium = 28, .high = 17 },
    { .low = 456, .medium = 763, .high = 456 },
    { .low = 99, .medium = 165, .high = 99 },
    { .low = 377, .medium = 631, .high = 377 },
    { .low = 346, .medium = 579, .high = 346 },
    { .low = 62, .medium = 104, .high = 62 },
    { .low = 96, .medium = 160, .high = 96 },
    { .low = 314, .medium = 526, .high = 314 },
    { .low = 201, .medium = 336, .high = 201 },
    { .low = 6, .medium = 9, .high = 6 },
    { .low = 9, .medium = 14, .high = 9 },
    { .low = 157, .medium = 262, .high = 157 },
    { .low = 944, .medium = 1581, .high = 944 },
    { .low = 26, .medium = 43, .high = 26 },
    { .low = 14, .medium = 22, .high = 14 },
    { .low = 314, .medium = 526, .high = 314 },
    { .low = 236, .medium = 394, .high = 236 },
    { .low = 31, .medium = 51, .high = 31 },
    { .low = 18, .medium = 30, .high = 18 },
    { .low = 188, .medium = 315, .high = 188 },
    { .low = 217, .medium = 363, .high = 217 },
    { .low = 211, .medium = 352, .high = 211 },
    { .low = 244, .medium = 408, .high = 244 },
    { .low = 385, .medium = 645, .high = 385 },
    { .low = 433, .medium = 724, .high = 433 },
    { .low = 330, .medium = 552, .high = 330 },
    { .low = 417, .medium = 698, .high = 417 },
    { .low = 501, .medium = 838, .high = 501 },
    { .low = 398, .medium = 666, .high = 398 },
    { .low = 1181, .medium = 1977, .high = 1181 },
    { .low = 1181, .medium = 1977, .high = 1181 },
    { .low = 1055, .medium = 1766, .high = 1055 },
    { .low = 110, .medium = 160, .high = 110 },
    { .low = 160, .medium = 220, .high = 160 },
    { .low = 190, .medium = 270, .high = 190 },
    { .low = 220, .medium = 300, .high = 220 },
    { .low = 230, .medium = 320, .high = 230 },
    { .low = 240, .medium = 340, .high = 240 },
    { .low = 250, .medium = 340, .high = 250 },
    { .low = 260, .medium = 360, .high = 260 },
    { .low = 270, .medium = 370, .high = 270 },
    { .low = 290, .medium = 410, .high = 290 },
    { .low = 300, .medium = 420, .high = 300 },
    { .low = 300, .medium = 420, .high = 300 },
    { .low = 320, .medium = 450, .high = 320 },
    { .low = 330, .medium = 460, .high = 330 },
    { .low = 330, .medium = 460, .high = 330 },
    { .low = 350, .medium = 490, .high = 350 },
    { .low = 360, .medium = 500, .high = 360 },
    { .low = 370, .medium = 520, .high = 370 },
    { .low = 400, .medium = 550, .high = 400 },
    { .low = 420, .medium = 580, .high = 420 },
    { .low = 450, .medium = 630, .high = 450 },
    { .low = 460, .medium = 640, .high = 460 },
    { .low = 500, .medium = 690, .high = 500 },
    { .low = 520, .medium = 730, .high = 520 },
    { .low = 540, .medium = 760, .high = 540 },
    { .low = 560, .medium = 780, .high = 560 },
    { .low = 580, .medium = 800, .high = 580 },
    { .low = 580, .medium = 810, .high = 580 },
    { .low = 590, .medium = 830, .high = 590 },
    { .low = 630, .medium = 880, .high = 630 },
    { .low = 690, .medium = 960, .high = 690 },
    { .low = 750, .medium = 1040, .high = 750 },
    { .low = 810, .medium = 1130, .high = 810 },
    { .low = 840, .medium = 1170, .high = 840 },
    { .low = 860, .medium = 1210, .high = 860 },
    { .low = 950, .medium = 1330, .high = 950 },
    { .low = 360, .medium = 540, .high = 360 },
    { .low = 400, .medium = 600, .high = 400 },
    { .low = 110, .medium = 150, .high = 110 },
    { .low = 117, .medium = 160, .high = 117 },
    { .low = 125, .medium = 171, .high = 125 },
    { .low = 133, .medium = 181, .high = 133 },
    { .low = 140, .medium = 192, .high = 140 },
    { .low = 148, .medium = 202, .high = 148 },
    { .low = 158, .medium = 216, .high = 158 },
    { .low = 168, .medium = 229, .high = 168 },
    { .low = 178, .medium = 243, .high = 178 },
    { .low = 188, .medium = 256, .high = 188 },
    { .low = 198, .medium = 270, .high = 198 },
    { .low = 209, .medium = 285, .high = 209 },
    { .low = 220, .medium = 300, .high = 220 },
    { .low = 231, .medium = 315, .high = 231 },
    { .low = 242, .medium = 330, .high = 242 },
    { .low = 253, .medium = 345, .high = 253 },
    { .low = 470, .medium = 850, .high = 470 },
    { .low = 400, .medium = 700, .high = 400 },
    { .low = 126, .medium = 296, .high = 126 },
    { .low = 600, .medium = 800, .high = 600 },
    { .low = 400, .medium = 700, .high = 400 },
    { .low = 62, .medium = 147, .high = 62 },
    { .low = 6300, .medium = 7500, .high = 6300 },
    { .low = 6500, .medium = 7800, .high = 6500 },
    { .low = 7000, .medium = 8500, .high = 7000 },
    { .low = 286, .medium = 390, .high = 286 },
    { .low = 341, .medium = 465, .high = 341 },
    { .low = 368, .medium = 502, .high = 368 },
    { .low = 379, .medium = 517, .high = 379 },
    { .low = 390, .medium = 532, .high = 390 },
    { .low = 396, .medium = 540, .high = 396 },
    { .low = 1200, .medium = 1420, .high = 1200 },
    { .low = 1230, .medium = 1480, .high = 1230 },
    { .low = 1330, .medium = 1610, .high = 1330 },
    { .low = 395, .medium = 661, .high = 395 },
    { .low = 129, .medium = 216, .high = 129 },
    { .low = 20, .medium = 33, .high = 20 },
    { .low = 370, .medium = 619, .high = 370 },
    { .low = 403, .medium = 675, .high = 403 },
    { .low = 147, .medium = 245, .high = 147 },
    { .low = 25, .medium = 41, .high = 25 },
    { .low = 402, .medium = 673, .high = 402 },
    { .low = 362, .medium = 605, .high = 362 },
    { .low = 417, .medium = 698, .high = 417 },
    { .low = 62, .medium = 104, .high = 62 },
    { .low = 29, .medium = 48, .high = 29 },
    { .low = 181, .medium = 302, .high = 181 },
    { .low = 31, .medium = 51, .high = 31 },
    { .low = 181, .medium = 302, .high = 181 },
    { .low = 400, .medium = 669, .high = 400 },
    { .low = 31, .medium = 51, .high = 31 },
    { .low = 423, .medium = 708, .high = 423 },
    { .low = 33, .medium = 54, .high = 33 },
    { .low = 425, .medium = 711, .high = 425 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
    { .low = 0, .medium = 0, .high = 0 },
};

#ifdef __cplusplus
}
#endif
