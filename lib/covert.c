#include "covert.h"
#include "eviction.h"
#include "utils.h"

void send_bit(int bit, EvictionSet *evset) {
  if (bit)
    access_set(evset);
}

int recv_bit(uint8_t *target, int threshold) {
  return time_load(target) > threshold;
}
