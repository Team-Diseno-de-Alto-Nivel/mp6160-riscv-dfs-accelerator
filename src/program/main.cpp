// RISC-V program entry point (SW-1): runs the harness over the DFS algorithms.

#include <cstdio>

#include "harness/harness.h"

int main() {
    dfs::Harness harness;

    // TODO(SW-1): const auto results = harness.run_all(); print comparative metrics.
    (void)harness;

    printf("DFS RISC-V baseline — harness skeleton\n");
    return 0;
}
