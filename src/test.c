#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <x86intrin.h>

#include "../lib/constants.h"
#include "../lib/covert.h"
#include "../lib/eviction.h"
#include "../lib/utils.h"

#define TRIALS 1000
#define KBD_KEYCODE_ADDR 0x716220106b20
#define WAIT_INTERVAL 10000

void *mapping_start;

void handle_sigint(int sig) { munmap(mapping_start, EVERGLADES_LLC_SIZE << 4); }
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
  NumList *list = new_num_list(EVERGLADES_ASSOCIATIVITY);
  probe(es, list);
  print_num_list(list);
  clear_num_list(list);

  sleep(1);
  access_set(es);
  probe(es, list);
  print_num_list(list);

  free(target);
  deep_free_es(es);
  munmap(mapping_start, EVERGLADES_LLC_SIZE << 4);
}

void test_find_all_eviction_sets(int set) {
  printf("set: %d\n", set);
  mapping_start = mmap(NULL, EVERGLADES_LLC_SIZE << 4, PROT_READ,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
  if (mapping_start == MAP_FAILED) {
    perror("map");
    return;
  }
  EvictionSet **es_list = get_all_slices_eviction_sets(mapping_start, set);
  printf("Reduction complete: ready to prime+probe\n");

  sleep(5);
  int threshold = threshold_from_flush((uint8_t *)es_list[0]->head);
  printf("threshold: %d\n", threshold);
  unsigned int core_id = 0;
  NumList *list = new_num_list(16);
  NumList *sliced_list_median[4];
  int candidates[4];

  for (int i = 0; i < 4; i++) {
    sliced_list_median[i] = new_num_list(100);
    candidates[i] = 0;
  }
  for (int j = 0; j < 100; j++) {
    for (int i = 0; i < 4; i++) {
      access_set(es_list[i]);
      uint64_t start = __rdtscp(&core_id);
      while (__rdtscp(&core_id) - start < 10000)
        ;
      probe(es_list[i], list);
      push_num(sliced_list_median[i], max(list));
      if (max(list) < threshold) {
        candidates[i]++;
      }
      clear_num_list(list);
    }
  }

  for (int i = 0; i < 4; i++) {
    CacheLine *iter = es_list[i]->head;
    for (int j = 0; j < es_list[i]->size; j++) {
      printf("%d ", get_i7_2600_slice(pointer_to_pa((void *)iter)));
      iter = iter->next;
    }
    printf("\n");
  }

  for (int i = 0; i < 4; i++) {
    printf("slice %d median: %ld, mean: %ld\n", i,
           median_and_sort(sliced_list_median[i]), mean(sliced_list_median[i]));
    printf("candidate %d: %d\n", i + 1, candidates[i]);
    free_num_list(sliced_list_median[i]);
  }
  free_num_list(list);
  free_es_list(es_list);
  munmap(mapping_start, EVERGLADES_LLC_SIZE << 4);
}

void measure_access(void) {
  mapping_start = mmap(NULL, EVERGLADES_LLC_SIZE << 4, PROT_READ,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
  if (mapping_start == MAP_FAILED) {
    perror("map");
    return;
  }
  printf("mmap: %p\n", (void *)mapping_start);

  volatile uint8_t data = *(volatile uint8_t *)(mapping_start);
  int set = pa_to_set(KBD_KEYCODE_ADDR, EVERGLADES);
  int slice = get_i7_2600_slice(KBD_KEYCODE_ADDR);
  printf("set: %d, slice: %d\n", set, slice);
  CacheLineSet *cl_set = new_cl_set();
  get_eviction_set_from_slices(KBD_KEYCODE_ADDR, EVERGLADES_ASSOCIATIVITY,
                               &mapping_start, &cl_set);
  EvictionSet *es = new_eviction_set(cl_set);
  NumList *nl = new_num_list(16);

  printf("please start typing\n");

  unsigned int core_id = 0;
  sleep(5);
  for (int i = 0; i < 100; i++) {
    access_set(es);
    uint64_t start = __rdtscp(&core_id);
    while (__rdtscp(&core_id) - start < 10000)
      ;
    probe(es, nl);
    print_num_list(nl);
    clear_num_list(nl);
  }
  deep_free_es(es);
  free_num_list(nl);

  // deep_free_cl_set(cl_set);
  munmap(mapping_start, EVERGLADES_LLC_SIZE << 4);
}

int main() {
  // test_eviction_set();
  // test_covert_channel();
  // test_eviction_and_pp();
  // test_find_all_eviction_sets(set);
}
