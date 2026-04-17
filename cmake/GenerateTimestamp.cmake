string(TIMESTAMP NOW "%Y-%m-%d %H:%M:%S" UTC)
file(WRITE "${OUTPUT_FILE}" "#pragma once\n#define PH_BUILD_TIMESTAMP \"${NOW}\"\n")
