#include <cstdio>
#include <string>

#include "harness/harness.h"
#include "harness/report.h"

int main(int argc, char** argv) {
    dfs::Tier tier = dfs::Tier::Legacy;

#ifndef DFS_BARE_METAL
    const std::string prefix = "--tier=";
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg.rfind(prefix, 0) != 0) continue;
        const std::string value = arg.substr(prefix.size());
        if (!dfs::parse_tier(value, tier)) {
            printf("unknown tier: %s (use legacy|small|medium|large)\n", value.c_str());
            return 2;
        }
    }
#else
    (void)argc;
    (void)argv;
#endif

    dfs::Harness harness(tier);
    const auto results = harness.run_all();
    return dfs::report(results) ? 0 : 1;
}
