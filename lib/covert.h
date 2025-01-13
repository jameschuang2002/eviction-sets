#include "eviction.h"
#include <stdint.h>

void send_bit(int bit, EvictionSet *evset);
int recv_bit(uint8_t *target, int threshold);
