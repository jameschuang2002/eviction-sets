#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "../lib/constants.h"
#include "../lib/covert.h"
#include "../lib/eviction.h"
#include "../lib/utils.h"

#define TRIALS 1000
#define KBD_KEYCODE_ADDR 0x6e0b03306b20

void test_eviction_set(void) {
  uint8_t victim = 0x37;
  CacheLineSet *cl_set = new_cl_set();

  uint64_t threshold = threshold_from_flush(&victim);
  if (!get_minimal_set(&victim, &cl_set, threshold)) {
    printf("Error. Failed to generate minimal eviction set.\n");
  }

  // Bring the victim into the cache
  uint8_t var2 = victim ^ 0xF7;

  // Access eviction set
  NumList *timings = new_num_list(TRIALS);
  evict_and_time(cl_set, &victim, timings, true);

  // Report median and mean times
  printf("Timings after accessing eviction set:\n");
  print_stats(timings);

  int evictions = 0;

  for (int i = 0; i < TRIALS; i++) {
    if (timings->nums[i] > threshold) {
      evictions++;
    }
  }

  printf("Successfully evicted %u/%u trials.\n", evictions, TRIALS);

  deep_free_cl_set(cl_set);
  free_num_list(timings);
}

void test_covert_channel(void) {
  uint8_t *target = malloc(sizeof(uint64_t *));
  printf("target: %p\n", target);
  CacheLineSet *cl_set = new_cl_set();
  int threshold = threshold_from_flush(target);
  if (!get_minimal_set(target, &cl_set, threshold)) {
    printf("Error. Failed to generate minimal eviction set.\n");
  }

  EvictionSet *evset = new_eviction_set(cl_set);
  for (int i = 0; i < 26; i++) {
    char c = 'a' + i;
    char recv = 0;
    for (int j = 0; j < 8; j++) {
      int bit = c & 0x1;
      send_bit(bit, evset);
      recv += recv_bit(target, threshold) << j;
      printf("%d ", bit);
      c >>= 1;
    }
    printf("%c\n", recv);
  }
  // deep_free_cl_set(cl_set);
  // deep_free_es(evset);
}

void test_sliced_evset(void) {
  uint8_t *target = malloc(sizeof(uint8_t *));
  printf("target: %p\n", target);
  uintptr_t pa = pointer_to_pa(target);

  // map 64 2 MB pages to get 128 candidate lines
  void *mapping_start = mmap(NULL, EVERGLADES_LLC_SIZE << 4, PROT_READ,
                             MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
  if (mapping_start == MAP_FAILED) {
    perror("map");
    return;
  }

  printf("memory mapped: %p\n", mapping_start);

  CacheLineSet *cl_set;
  get_eviction_set_from_slices(pa, 16, &mapping_start, &cl_set);

  for (int i = 0; i < cl_set->size; i++) {
    printf("%p\n", cl_set->cache_lines[i]);
  }

  // EvictionSet *es = new_eviction_set(cl_set);
  *target;
  for (int i = 0; i < 16; i++) {
    *cl_set->cache_lines[i];
  }
  for (int i = 0; i < 16; i++) {
    *cl_set->cache_lines[15 - i];
  }
  int time = time_load(target);
  int threshold = threshold_from_flush(target);
  printf("threshold: %d, evict_time: %d\n", threshold, time);
  free(target);
  // deep_free_es(es);
  munmap(mapping_start, EVERGLADES_LLC_SIZE << 4);
}

void measure_keypress(void) {}

int main(void) {
  // test_eviction_set();
  // test_covert_channel();
  test_sliced_evset();
}
