#include "mosaic.h"
#include <cstdint>
#include <iostream>
int main() {
    static_assert(MOSAIC_C_API_VERSION_MAJOR == 0);
    if (mosaic_tokenizer_semantics_version() != 2u) return 2;
    std::cout << mosaic_version_string() << "\n";
    return 0;
}
