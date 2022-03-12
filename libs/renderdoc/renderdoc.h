#pragma once

namespace Renderdoc {
    void init();

    void start_capture(void *device);
    void stop_capture(void *device);
}