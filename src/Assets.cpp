#include "Assets.h"

#include <filesystem>
#include <vector>

#include <SDL2/SDL.h>

namespace {

std::string exe_dir() {
    char *base = SDL_GetBasePath();
    if (!base) return "";
    std::string d(base);
    SDL_free(base);
    return d;
}

}  // namespace

std::string resolve_asset(const std::string &relative) {
    namespace fs = std::filesystem;
    std::vector<std::string> candidates = {
        relative,
        std::string("./") + relative,
        std::string("../") + relative,
    };
    std::string ed = exe_dir();
    if (!ed.empty()) {
        candidates.push_back(ed + relative);
        candidates.push_back(ed + "../" + relative);
        candidates.push_back(ed + "../../" + relative);
    }
    for (const auto &c : candidates) {
        std::error_code ec;
        if (fs::exists(c, ec)) return c;
    }
    return relative;
}
