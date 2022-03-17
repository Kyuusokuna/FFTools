#pragma once
#include <Windows.h>
#include "renderdoc_app.h"

namespace Renderdoc {
    RENDERDOC_API_1_4_0 *rdoc_api = NULL;

    inline void init() {
        if (HMODULE mod = GetModuleHandleA("renderdoc.dll")) {
            pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
            if (RENDERDOC_GetAPI)
                RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void **)&rdoc_api);
        }
    }

    inline void start_capture(void *device) {
        if (rdoc_api) rdoc_api->StartFrameCapture(device, NULL);
    }

    inline void stop_capture(void *device) {
        if (rdoc_api) rdoc_api->EndFrameCapture(device, NULL);
    }
}