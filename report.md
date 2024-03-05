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

### Model Design
This model supports multi-processor acesses, requiring caches to invalidate corresponding cache lines when memory updates occur to ensure consistency. The processor is designed with the following configuration: it employs both read-allocate and write-allocate strategies, utilizes a write-through caching policy, and features an 8-way set associative cache that is managed using a Least Recently Used (LRU) policy.
The simulation consists of various components:
1. **Manager (Dispatcher):** This component initiates the system, controls its termination, assigns IDs to each CPU, creates the specified number of CPUs, and ends the system once every CPU has completed its tasks.
2. **CPU:** This component process operations from a trace file based on its ID, distinguishing between read, write, and no-operation (nop) tasks. It uses the cache interface for reading or writing data.
3. **Cache:** This component manages cache line states, synchronizes actions with other caches, accesses memory, and communicates over the bus.
4. **Bus:** This component handles requests from other parts (such as Cache and Memory), retrieving requests from a queue and dispatching them based on their source and destination at the negative edge of a cycle.
5. **Memory:** Simulating 100-cycle memory access operations, this component fetches memory access requests from a queue at the positive edge of a cycle, waits for 100 cycles, and then sends the access results back to the requesting component via the bus.

### Implementation Details
##### Message
Message is composed of two components: request and request_id
```
typedef struct request {
    uint8_t cpu_id; // cpu no.
    enum location source;
    enum location destination;
    enum op_type op;
    uint64_t addr;
} request;

typedef struct request_id {
    uint8_t cpu_id; // cpu no.
    enum location source;
} request_id;

```
The communication protocol uses two structs to facilitate burst requests, which are stored in the write buffers of both the cache and memory. Each request is identified by a request_id, allowing it to be located in the appropriate write buffer in the cache or memory. A burst request is a single atomic operation that consists of several individual requests, designed to be executed without interruption by other requests.
Each request encapsulates all essential information required for data transfer and state transitions, enabling the bus to route messages according to their origin and destination. Priority is implicitly determined by the source of the request, eliminating the need for an explicit priority flag.
##### Cache
Cache manages the states of the cache lines by receive and send requests through bus. The Cache defined several interfaces for other components to communicate with it.
```
class cache_if : public virtual sc_interface {
    public:
    virtual int cpu_read(uint64_t addr) = 0;

    virtual int cpu_write(uint64_t addr) = 0;

    virtual int state_transition(request req) = 0;

    virtual int ack() = 0;

    virtual int finish_mem() = 0;

    virtual std::vector<request> get_requests() = 0;
};
```
I will explain the Cache implementaion details in a read & write lifecycle order.
###### Read
The cpu uses cpu_read to drive the cache read data from the cache line based on the address. When the cache gets a read hit or a read miss, the cache will broadcast the event with all the nessary infomration, and wait the request to be served. After the 

The cache will check the ack_ok flag to see if the bus already processed its request. The bus will use the ack() interface to tell the CPU the request is sent. The bus sent the ack at the negative edge, and the CPU checks it at the positive edge, this synchronization will ensure that all the other caches already notice the broadcast when the cache knows the ack.
###### Write

###### States Transition
The transition between two states, from invalid to valid and from valid to invalid, plays a crucial role in maintaining the accuracy of simulations. Understanding the timing of these transitions is essential for the system's correctness.
* Transition from Invalid to Valid 
A cache transitions from invalid to valid under two circumstances, both of which ensure it reflects the latest data in memory.
    * Read-Allocate
    This occurs when a cache misses on a read operation and must fetch new data from memory. The cache is considered valid after it receives acknowledgment (ack) from the bus that its request has been processed, but before it actually receives the data from memory.

    * write-allocate & write-through
    During a write operation, the cache marks the data as valid with similar timing to read-allocate: after receiving the ack but before the memory responds. This applies to burst write requests (comprising write hit and write-through or write miss, write allocate, and write-through). The burst request prevents later-served caches write from interrupting the memory access. 
    
    The detailed reason behind these specific timing is in the write race analysis section.
* Valid -> Invalid
The invalidation process is happened at the negative edge when there is need to invalidate. The bus will call the the state_transition function defined by the cache_if to invalidate the cache line except for the sender, and the cache doesn't have actions at the negative edge, so it avoids the conflicts.
###### Write race analysis
When the cache initiates a write request, it must group the **memory access requests** with **broadcast requests** in a single burst request. While a cache is awaiting a response from the memory, there's a chance it may receive a notification that the cache line it is waiting for has been invalidated, indicating that the memory location has been overwritten by other caches. This process is regulated through sequential memory execution, where the memory serves all requests in a First-In, First-Out (FIFO) order. Therefore, a cache that attempts to write to the memory and occupies the bus later has the capability to overwrite the value at the same location for all previous write accesses. Since this cache occupies the bus later, it holds the final authority to invalidate other caches and maintain its validity. Grouping requests into a single burst request is crucial to prevent the sequencing of memory access requests from overlapping, otherwise, if an early write operation is announced to other caches but executed later, it could mistakenly overwrite a newer write operation from other caches.

##### Bus

##### Memory
The memory has a queue contains the requests from the cache.
```
typedef std::vector<request_id> bus_requests;
```
The memory processes the requests sequentially one by one, and one request needs 100 cycles to finish. The execution order matters because it reflects the serving order of the bus, the execution order must matches the order that the write request is processed by the bus, because the cache relies on this assumption.
The bus will put the request into the memory buffer one by one at the negative clock edge, the memory will has thread that fetches the task from the buffer at the positive clock edge and there is no ruuning task.


### Experiments
##### Performance Evalution with Valid/Invalid Memory Model 
##### Observing Cache Hit Rates with Snoop Enabled and Disabled.
##### Assessing the Impact of Enabling vs. Disabling Memory Priority.
##### Evaluating Performance with Multi-Channel Memory Configuration.

