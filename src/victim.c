#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <x86intrin.h>

#include "../lib/eviction.h"

uint8_t *target;

unsigned int core_id = 0;

void handle_sigint(int sig) { free(target); }

void send(int bit) {
  uint64_t start_time = __rdtscp(&core_id);
  if (bit) {
    while (__rdtscp(&core_id) - start_time < 10000) {
      *(volatile uint8_t *)target;
    }
  } else {
    while (__rdtscp(&core_id) - start_time < 10000)
      ;
  }
}

void send_byte(uint8_t byte) {
  for (int i = 0; i < 8; i++) {
    send(byte & 0x1);
    byte >>= 1;
  }
}

int main() {
  target = malloc(sizeof(uint8_t *));
  uintptr_t pa = pointer_to_pa(target);
  printf("%p\n", (void *)pa);

  FILE *pa_file;
  pa_file = fopen("pa.txt", "w");
  if (pa_file == NULL) {
    perror("fopen");
    exit(1);
  }

  fprintf(pa_file, "%lx", pa);
  printf("physical address write file\n");
  fclose(pa_file);

  while (1) {
    volatile uint8_t tmp = *target;
  }
  return 0;
}
