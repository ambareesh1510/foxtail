#include "fs.h"

char fs[NUM_BLOCKS * BLOCK_SIZE] = {
    // TODO: temporary!
#embed "fs.bin"
};
