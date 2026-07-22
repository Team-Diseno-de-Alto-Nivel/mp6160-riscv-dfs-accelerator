#include "harness/harness.h"
#include "harness/report.h"

int main() {
    dfs::Harness harness;
    const auto results = harness.run_all();
    return dfs::report(results) ? 0 : 1;
}
