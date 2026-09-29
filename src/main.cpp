#include <cstdlib>
#include <cstring>

#include "core/app.h"

int main(int argc, char** argv) {
    aldoria::AppOptions options;
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        bool hasValue = i + 1 < argc;
        if (!std::strcmp(a, "--script") && hasValue) options.scriptPath = argv[++i];
        else if (!std::strcmp(a, "--shots") && hasValue) options.shotsDir = argv[++i];
        else if (!std::strcmp(a, "--max-seconds") && hasValue) options.maxSeconds = (float)std::atof(argv[++i]);
        else if (!std::strcmp(a, "--mute")) options.mute = true;
        else if (!std::strcmp(a, "--unmute")) options.unmute = true;
        else if (!std::strcmp(a, "--volume") && hasValue) options.masterVolume = (float)std::atof(argv[++i]);
    }
    aldoria::App app(options);
    return app.run();
}
