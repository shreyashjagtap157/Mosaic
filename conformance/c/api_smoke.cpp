#include "mosaic.h"
#include <cstdint>
#include <iostream>
int main() {
    static_assert(MOSAIC_C_API_VERSION_MAJOR == 1, "Mosaic C API major version must remain 1");
    if (mosaic_tokenizer_semantics_version() != 2u) return 2;
    std::cout << mosaic_version_string() << "\n";
    return 0;
}
