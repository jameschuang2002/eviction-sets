# Eviction Sets

This repository contains C code for generating and minimizing eviction sets, as well as a number of related functions. It implements an efficient algorithm for generating an eviction set for a particular victim address, without knowledge of cache slicing or physical address mappings. It also implements an eviction set reduction algorithm which uses group testing to efficiently reduce large eviction sets to minimal ones  (the algorithm is from [Theory and Practice of Finding Eviction Sets](https://arxiv.org/pdf/1810.01497)).

This code can be used as a foundation to carry out cross-process Prime+Probe attacks on modern Intel systems, without root, huge pages, or knowledge of the LLC slicing function.

Thank you to [Stephan van Schaik](https://codentium.com/about/) for some implementation details of the reduction algorithm.
