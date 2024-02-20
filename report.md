# Lab Report: Cache Coherency

* Student: Zhiyang Wang
* Student ID: 15327388

### Introduction
In this assignment I will use SystemC to build a simulator of a Level-1 Data-cache and various implementations of a cache coherency protocol and evaluate their performance.
In Assignment 1, I implemented a single 32kB 8-way set-associative L1 D-cache with a 32-byte line size, featuring a least-recently-used (LRU) write-back replacement strategy together with an allocate-on-write policy. The LRU policy is implemented by an array-based doubly linked list, and the memory access and the cache access are simulated with different latencies (100s and 1s, respectively).
### Implmentation of a Single Cache
##### Decomposing a 64-Bit Address for Cache Management
For a 64-bit address, we can partition it into three components to accommodate our 32kB 8-way set-associative cache: the tag, set index, and block offset.
1. block offset
The cache line size is 32 bytes long, we need a offset to determine the location of the byte in the cache line. To allocate the requested byte by the address, we need at least 5 bits to represent the offset of the byte in the cache line. In my code, I use a bitmask to extract the block offset part from the address.
```
size_t offset = (addr & 0b11111);
```
2. set index
The cache is organized into 8-way sets with a total capacity of 32kB, and each set contains eight cache lines, each 32 bytes in size. I need this part of number to get the location of the cache set, and the total number of sets can be calculated as $ (32 \times 1024 \div (32 \times 8)) = 128$. I need at least 7 bits to represent the set index in the cache, I didn't use bitmask to extract the set part but the '%' operation in the code functions the same as masking the lower 7 bits. 
```
static const size_t NR_SETS = CACHE_SIZE / (SET_SIZE * BLOCK_SIZE);
...
uint64_t set_i = (addr >> 5) % NR_SETS; // NR_SETS is 128.
```
3. tag
This section represents the remainder of the address, which amounts to 64 - (bits for the set index) - (bits for the offset) = 64 - (7 + 5) = 52 bits. Tags are utilized to identify the appropriate cache line within the 8-way sets. If the tags match, it indicates that the data resides in this cache. Otherwise, it signifies the necessity to either evict the least recently used (LRU) cache line or occupy a new one.
```
uint64_t tag = (addr >> 5) / NR_SETS;
```
##### Implementation of the LRU policies
I desgined an array-based doubly linked list to implement the LRU (Least Recently Used) policy, it uses a head that points to the most recently used LRU unit at the front of the linked list, and a tail that points to the least recently used unit at the end. Whenever a cache line is accessed, it is virtually moved to the front of the list. This process involves no actual data movement; instead, the 'prev' and 'next' pointers are adjusted to maintain the units' positions within the list, and the units themselves remain fixed in their array positions, and the linked list structure is defined as follow.
Whenever a tag mismatch occurs and the cache lines are fully occupied, the tail element of the linked list can be evicted to leave a new space for the new data.

```
struct LRUnit {
    bool dirty;
    uint8_t index;
    uint64_t tag;
    uint8_t data[BLOCK_SIZE];
    LRUnit* next;
    LRUnit* prev;
};

...
class LRU {
    ...
    LRUnit* head;
    LRUnit* tail;
    LRUnit* lines;
    uint8_t lru_index;
}
```
##### Implementation of the write back and allocate-on-write policy
The write-back policy indicates that data in a cache line is written back to memory only if the "dirty" flag for that cache line is set to true at the time of eviction as demonstrated in the previous code. In my code, this model is showed when handling dirty cache lines during eviction:
1. If a cache line is marked as dirty (curr->dirty is true), it indicates that the cache line has been modified but not yet written back to the main memory.
2. Before evicting a dirty cache line (to make space for a new one or due to an LRU policy), the code simulates a write-back operation with a latency (sc_core::wait(100)). This latency represents the time taken to write the dirty cache line back to the main memory. 

The allocate-on-write policy indicates that every write miss we need to fetch the data from the memory and allocate a new space for it in the cache, so I use 100-second latency to simulate the allocation operation after every write miss.
1. On a write miss, the code simulates a memory reading, and then the code checks if the cache is full. If not, it allocates a new cache line (this->lines[this->size]) and updates it with the new tag and marks it as dirty, indicating that it contains data not yet written to the main memory.
2. If the cache is full, it evicts the least recently used (LRU) cache line, this involes some latencies. If the evicted cache line is dirty, it first simulates writing back to the main memory before allocating the cache line to the new data, we also need to mark the new cache line as dirty.

For each cache hit, the code simulates cache access for simplicity. Since the cache access time is significantly shorter than memory access time, the code does not simulate additional cache latency in further operations after the inital latency.


##### Performance of a Single Cache
| tracefiles | readmiss | readhit | writemiss | writehit | hitrate | latency |
|-----------------------|--------------------------|--------------------------|--------------------------|---------------------------|---------------------------|---------------------------|
| dbg_p1                   | 21 | 19 | 34 | 26 | 45.000000 | 5645 ns |
| fft_1024_p1              | 2982 | 503437 | 994 | 152997 | 99.397950 | 1759444 ns |
| matrix_mult_50_50_p1     | 939 | 1631711 | 2 | 255148 | 99.950154 | 3868759 ns |
| matrix_vector_200_200_p1 | 10053 | 631147 | 52 | 80748 | 98.600416 |  2448895 ns |
| matrix_vector_5000_8_p1  | 10005 | 659995 | 1252 | 659995 | 98.538052 | 2768143 ns |
| matrix_vector_8_5000_p1  | 20003 | 620045 | 12 | 80020 | 97.220448 | 4162740 ns |

The results indicate a high hit rate when data operations adhere to spatial and temporal locality principles. With its configuration (8-way set associative LRU with write back and write allocate policies), the cache achieves an average hit rate of above 98 percent across most trace files.

