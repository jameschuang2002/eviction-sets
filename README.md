# Eviction Sets

This repository contains C code for generating and minimizing eviction sets, as well as a number of related functions. It implements an efficient algorithm for generating an eviction set for a particular victim address, without knowledge of cache slicing or physical address mappings. It also implements an eviction set reduction algorithm which uses group testing to efficiently reduce large eviction sets to minimal ones  (the algorithm is from [Theory and Practice of Finding Eviction Sets](https://arxiv.org/pdf/1810.01497)).

This code can be used as a foundation to carry out cross-process Prime+Probe attacks on modern Intel systems, without root, huge pages, or knowledge of the LLC slicing function.

`lib/eviction.c` contains most of the code for eviction set generation, reduction, and traversal. `lib/utils.c` contains generic functionality for manipulating lists of access times and computing statistics.

Thank you to [Stephan van Schaik](https://codentium.com/about/) for some implementation details of the reduction algorithm.

## Example

To see a simple example which generates a large eviction set for a variable, then reduces it to its minimal core, and finally measures the eviction rate, run `./test.sh`.

## Using this library with your own code

To use this library in your own project, simply clone the repository and include `lib/utils.h`. Make sure to specify the correct path depending on where you clone the repository. `test.c` is a simple example which generates an eviction set for a victim variable, minimizes the set, and tests how effectively the set evicts the victim. It can easily be built upon.

Note that this code is only intended to work on Intel machines running Linux. It was exclusively tested on Intel Coffee Lake and Skylake architectures on Ubuntu 22.04 LTS.

### `CacheLineSet` vs `EvictionSet`

In this library, a `CacheLineSet *` points to a struct with a size and a linear list of `CacheLine *`s. In contract an `EvictionSet *` uses an intrusive linked-list implementation, allowing you to traverse an eviction set without accessing irrelevant cache lines in the process.

### Measuring cache hit threshold

To measure the cache hit threshold on your system, use `threshold_from_flush()`:

```C
uint8_t dummy;
uint64_t threshold = threshold_from_flush(&dummy);
```

### Generating an eviction set (not minimal)

To generate an eviction set for an address in your own address space, simply pass that address to `inflate()`, along with the size of the eviction set, the number of samples when testing it, and the cache hit threshold for your system:

```C
CacheLineSet *cl_set = inflate(victim, INITIAL_SIZE, SAMPLES, threshold);
```

Suggested values would be an initial size of 8192 and 100 samples.

To generate a minimal eviction set directly (without having to later reduce it), use `generate_set()`:

```C
uint8_t victim;
EvictionSet *es = generate_set(&victim);
```

### Traversing an eviction set

Eviction sets can be traversed with `access_set()`:

```C
EvictionSet *es = new_eviction_set(cl_set);
access_set(es);
```


### Reducing an eviction set to its minimal core

To reduce a large eviction set to its minimal core (n cache lines for a n-way cache), use `reduce2()`:

```C
CacheLineSet *reserve = new_cl_set();
bool result = reduce2(cl_set, reserve, victim, SAMPLES, threshold, BINS);
deep_free_cl_set(reserve);
```

### Testing eviction sets

To test how well an eviction set evicts a particular victim, use `evict_and_time()`:

```C
NumList *timings = new_num_list(TRIALS);
evict_and_time(cl_set, &victim, timings, true);

// Report median and mean times
print_stats(timings);
```

This will report the mean, median, and standard deviation access times for the victim address immediately after accessing the eviction set.

## Guide for future development

This eviction set library contains the beginnings of a Prime+Probe implementation. The next major goal would be to fully implement cross-process Prime+Probe, which would be split into the following stages:

1. **Measuring cache traces:** given temporary knowledge of the victim physical address and thus the corresponding cache set, measure the access latency at regular intervals, saving the results to a file.
2. **Generating all probe sets:** given the number of unknown physical address bits, generate this many unique eviction sets (sets that don't evict each other) and minimize them.
3. **Comparing measured trace to saved trace**: while the victim is running, measure the access latency of each eviction set at regular intervals, comparing the measurements with the results saved in part 1.
4. **Evicting the victim:** once the correct eviction set has been determined, access it as fast as possible to ensure the victim is evicted.

Stage 2 is already been mostly implemented in `generate_sets()` (finer details of stage 2 are discussed in the comments to `generate_sets()`), and stage 4 can just use `access_set()`. Thus, the bulk of the work in converting the existing code into a practical Prime+Probe implementation lies in measuring and comparing cache traces.
