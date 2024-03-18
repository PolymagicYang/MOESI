# Lab3 Report: Implement the MOESI Protocol

* Student: Zhiyang Wang
* Student ID: 15327388

### Introduction
In this lab, I implemented the MOESI protocol for multicore processor systems. The MOESI protocol is designed to maintain cache coherence by ensuring that every cache line is in one of the following states: Modified, Owned, Exclusive, Shared, or Invalid. Like in previous labs, the cache uses the Least Recently Used (LRU) method for eviction, combined with a write-back policy
### Components implementations
I updated the bus system and certain aspects of the cache while keeping the rest of the components the same with previous labs. To better handle messages from other caches, I introduced probe threads. These threads manage changes in cache line states and perform cache-to-cache data transfers based on incoming messages.
To address timing issues, I added a write buffer to the cache. Since both the cache probe thread and the bus operate during the negative clock edge, placing messages directly into the bus queue could lead to conflicts. By using a write buffer, messages are queued and then sent out on the next positive clock edge, avoiding these issues.
I also implemented new interfaces in the cache to allow other components to easily access cache line information, enhancing system interoperability.
The bus logic has been streamlined to efficiently process only three types of operations: read probe, write probe, and data-transfer. For read probe, the bus identifies the most current data holder and routes the data appropriately. Write operations are directed based on the intended recipient: they can either be broadcast to all or specifically sent to memory. Data transfer operations are simplified to send data directly to the requester, whether it’s from another cache or memory, thereby reducing unnecessary data broadcasting.
The 'burst' concept introduced in my previous lab has been discontinued due to its inferior performance and the difficulties it presents when applied within the MOESI protocols.

### Model Impelemntations
##### Bus-Master Probes and Cache Coherency State Transitions in MOESI Protocol defined in AMD64 MOESI Specs:
*There are two general types of bus-master probes:*
*1. Read probes indicate the external master is requesting the data for read purposes.*
*2. Write probes indicate the external master is requesting the data for the purpose of modifying it.*
*State transitions involving read misses and write misses can cause the processor to generate probes into external bus masters and to read main memory.*
*Read hits do not cause a MOESI-state change. Write hits generally cause a MOESI-state change into the modified state. If the cache line is already in the modified state, a write hit does not change its state.*

##### Cache Coherency State Transitions Implementation
The bus operations within this implementation adhere to the protocols specified in the AMD64 manual. Specifically, bus interactions are initiated only during read misses, write misses, and certain write hits. Conversely, read hits and most write hits do not trigger bus operations since there is no necessity to access memory data. 
##### States Transition On Hits
For read hits, the cache line's data is already available locally, eliminating the need to notify other caches of the read operation. The behavior for write hits, however, varies depending on the cache line's current state. If the cache line is in an exclusive or modified state, it implies only ownership of the current version of the data, and therefore, there is no requirement to broadcast a write probe to invalidate other caches. On the other hand, if the cache line is in a shared or owned state, it implies that the data is not unique to this cache, which means it needs to send write probes to invalidate the other caches that share the data.
Whenever the cache line probe thread detects a write probe from another cache, it will invalidate the corresponding cache line. This invalidation process occurs on the falling edge of the clock cycle.

##### States Transition On Read Miss 
This read probe only occurs when there's a read miss or write miss (depends on my implementation), and it depends on the state in the other caches. The cache sends out a read probe to the bus, and the bus checks to see which cache has the most up-to-date data for that particular address. If no cache currently has the data, the bus requests it from the main memory. If a cache does have the data, it responds to the read probe. If the responding cache's state for the cache line was 'Exclusive', it will change it to 'Shared'. If the cache line was 'Modified', it will change to 'Owned'. If the responding cache had the cache line in 'Owned' or 'Shared' state already, the state remains unchanged.

##### States Transition On Write Miss
I convert a write miss to a WB memory type into two separate MOESI-state changes. Initially, a read miss moves the cache line to either the exclusive or shared state. This is followed by a write hit. If the cache line is in the shared state during the write, a write probe is sent to other caches to update their status; however, if the cache line is in the exclusive state, there's no need to send out invalidation messages to other caches. Implementing the process this way lightens the logical workload and simplifies the bus dispatch logic.

##### States Transition When Evict the LRU Cache Line
When the cache line becomes full and needs to evict a cache line, it does so using the Least Recently Used (LRU) method. If the cache line being evicted is in a modified or owned state, its data must be written back to memory. In my implementation, while writing data to the cache line, it retains its owned or modified state, allowing other caches to continue accessing the correct data from it. Once the evicted cache line has written its data to memory, its state is changed to invalid, enabling other caches to read the most up-to-date data from memory.

#### Edge Cases
##### Read Race Conditions
In the MOESI design, a situation may arise where a cache sends a request to another cache believed to be in the correct state to provide the data. However, the targeted cache might still be awaiting the data from memory. This scenario is common in read race conditions, where the cache declared as the winner in read races is granted exclusive status before it actually receives the data, making the memory the true holder of the most recent data. Despite this, the bus will send a read probe to the exclusive cache, compelling it to update its state to shared appropriately, but it will direct the request for the actual data to the memory to ensure the retrieval of the most current data.

##### Write Back Race Condition (Invalid -> Modified)
Since the Write-Back (WB) action is decomposed into read miss and write hit, this situation is close to the upcoming scenario (Write Hit Race Condition) and can be addressed within that context.

##### Write Hit Race Conditions
When multiple caches attempt to write to a cache line that is currently shared, race conditions arise. In such cases, only one cache should successfully complete its write operation, essentially 'winning' the race. Consequently, all other caches must invalidate their versions of the cache line and initiate a write miss procedure. This cycle repeats until no copies of the cache line remain in a shared state, ensuring that at any given time, only one cache holds the cache line in a modified state.
###### Sanity Test:
The test file is named as ./4_proc_write_race.trf
(P0, READ, 0)
(P1, READ, 0)
(P2, READ, 0)
(P3, READ, 0)
1000 nops

(P1, WRITE, 0)
3 nops
(P2, WRITE, 0)
3 nops
(P3, WRITE, 0)
3 nops
(P0, WRITE, 0)

The test file is designed to bring all cache lines to a shared state before initiating simultaneous writes. This setup triggers write miss race conditions within the system, as each cache must secure the most up-to-date data for writing. This involves reading the latest data from the cache that wins the race then modifiy the data, since other caches might have already obtained the data from the winner. Subsequent write attempts will invalidate earlier ones.
To ensure that these restarts from write misses do not impact the system's hit rate, I implemented a flag to determine the hit rate statistics independently. The anticipated hit rate is calculated to be 50%, derived from each processor experiencing one read miss followed by one write hit.

Highlights output for each processors:
p0:
00000 s: cache_0: [READ MISS]
0002 ns: cache_0: [TRANSITION] Invalid -> Exclusive.
2500 ps: cache_0: [TRANSITION] Exclusive -> Shared,
0103 ns: cache_0: [DATA TRANSFER] Read data from memory.
1107 ns: cache_0: [WRITE HIT]
1108 ns: cache_0: [TRANSITION] Shared -> Modified
1108500 ps: cache_0: [TRANSITION] Modified -> Invalid

p1:
00000 s: cache_1: [READ MISS]
0005 ns: cache_1: [TRANSITION] Invalid -> Shared.
0106 ns: cache_1: [DATA TRANSFER] Read data from another cache.
1107 ns: cache_1: [WRITE HIT]
1107500 ps: cache_1: [TRANSITION] Shared -> Invalid,
1110 ns: cache_1: [WRITE RACE DETECTED] Restart from a write miss.
1110 ns: cache_1: [WRITE MISS]
1112 ns: cache_1: [TRANSITION] Invalid -> Exclusive.
1112500 ps: cache_1: [TRANSITION] Exclusive -> Shared,
1213 ns: cache_1: [DATA TRANSFER] Read data from memory.
1215 ns: cache_1: [TRANSITION] Shared to Modified.
1216500 ps: cache_1: [TRANSITION] Modified -> Invalid

p2:
00000 s: cache_2: [READ MISS]
0004 ns: cache_2: [TRANSITION] Invalid -> Shared.
0105 ns: cache_2: [DATA TRANSFER] Read data from another cache.
1107 ns: cache_2: [WRITE HIT]
1107500 ps: cache_2: [TRANSITION] Shared -> Invalid,
1111 ns: cache_2: [WRITE RACE DETECTED] Restart from a write miss.
1111 ns: cache_2: [WRITE MISS]
1113 ns: cache_2: [TRANSITION] Invalid to Shared.
1214 ns: cache_2: [DATA TRANSFER] Read data from another cache.
1214500 ps: cache_2: [TRANSITION] Shared -> Invalid,
1217 ns: cache_2: [WRITE RACE DETECTED] Restart from a write miss.
1217 ns: cache_2: [WRITE MISS]
1219 ns: cache_2: [TRANSITION] Invalid to Shared.
1320 ns: cache_2: [DATA TRANSFER] Read data from another cache.
1320500 ps: cache_2: [TRANSITION] Shared -> Invalid,
1322 ns: cache_2: [WRITE RACE DETECTED] Restart from a write miss.
1322 ns: cache_2: [WRITE MISS]
1323 ns: cache_2: [TRANSITION] Invalid -> Exclusive.
1424 ns: cache_2: [DATA TRANSFER] Read data from memory.
1424 ns: cache_2: [TRANSITION] Exclusive to Modified.

p3:
00000 s: cache_3: [READ MISS]
0003 ns: cache_3: [TRANSITION] Invalid -> Shared.
0104 ns: cache_3: [DATA TRANSFER] Read data from another cache.
1107 ns: cache_3: [WRITE HIT]
1107500 ps: cache_3: [TRANSITION] Shared -> Invalid,
1112 ns: cache_3: [WRITE RACE DETECTED] Restart from a write miss.
1112 ns: cache_3: [WRITE MISS]
1114 ns: cache_3: [TRANSITION] Invalid to Shared.
1214500 ps: cache_3: [TRANSITION] Shared -> Invalid,
1216 ns: cache_3: [DATA TRANSFER] Read data from another cache.
1216 ns: cache_3: [WRITE RACE DETECTED] Restart from a write miss.
1218 ns: cache_3: [TRANSITION] Invalid -> Exclusive.
1218500 ps: cache_3: [TRANSITION] Exclusive -> Shared,
1319 ns: cache_3: [DATA TRANSFER] Read data from memory.
1321 ns: cache_3: [TRANSITION] Shared to Modified.
1321500 ps: cache_3: [TRANSITION] Modified -> Invalid

The test results indicate that with four processors, the latency is 100ns. This outcome arises because my design partially addresses the issue. Ideally, the winning processor should cancel the write probes of others, allowing processors that need to restart to directly read up-to-date data from the cache, thereby maintaining data correctness. Although my design's performance does not match that of the ideal solution, it still behaves as expected. This is because, upon a restart, if a write operation invalidates all other caches, the most current data indeed resides in the memory, preventing any cache from accessing outdated information from a previous winner.
Regarding the number of restarts, the outcome aligns with my predictions: Cache_0, being the initial winner, does not require a restart. Processor 1, as the second winner, restarts once. Processor 3, the third winner, undergoes two restarts, while Processor 4 is the last to write, following the anticipated sequence.

##### Reading data when eviction
When the LRU (Least Recently Used) system evicts a cache line that is in an owned or modified state, it's important to maintain the state of that cache line until its data has been successfully written back to memory. This is because the memory's copy of the data is outdated, and the cache may still need to respond to read requests from other caches with the most current data. Implementing a test trace for this scenario is challenging, so I have only provided the expected behavior.

### Test Cases
##### Shared to Invalid
```
P0 READ 0
P1 READ 0
P2 READ 0
P3 READ 0

200 nops

P0 WRITE 0
P1 nops
P2 nops
P3 nops
```

| Manager | Reads | RHit | RMiss | Writes | WHit | WMiss | Hitrate  | MAccessTime | WaitBus |
|---------|-------|------|-------|--------|------|-------|----------|-------------|---------|
| 0       | 1     | 0    | 1     | 1      | 1    | 0     | 50.000000| 1           | 1.500000|
| 1       | 1     | 0    | 1     | 0      | 0    | 0     | 0.000000 | 1           | 5.000000|
| 2       | 1     | 0    | 1     | 0      | 0    | 0     | 0.000000 | 1           | 4.000000|
| 3       | 1     | 0    | 1     | 0      | 0    | 0     | 0.000000 | 1           | 3.000000|

Latency: 310 ns

Initially, all cache lines transition into shared states, followed by a 100 ns delay. 200 ns later, a write operation by one of the caches triggers the invalidation of the others. As a result, all caches experience a read miss initially. However, only one cache eventually achieves a write hit, aligning the hit rate with expectations.

##### Owned to Invalid

```
P0 READ 0
P1 READ 0
P2 READ 0
P3 READ 0

150 nops

P0 WRITE 0
P1 nops
P2 nops
P3 nops

P0 nops
P1 READ 0
P2 READ 0
P3 READ 0

P2 WRITE 0
```

The result is:
| Manager | Reads | RHit | RMiss | Writes | WHit | WMiss | Hitrate   | MAccessTime | WaitBus  |
|---------|-------|------|-------|--------|------|-------|-----------|-------------|----------|
| 0       | 1     | 0    | 1     | 1      | 1    | 0     | 50.000000 | 1           | 1.500000 |
| 1       | 2     | 0    | 2     | 0      | 0    | 0     | 0.000000  | 1           | 3.500000 |
| 2       | 2     | 0    | 2     | 1      | 1    | 0     | 33.333333 | 1           | 2.333333 |
| 3       | 2     | 0    | 2     | 0      | 0    | 0     | 0.000000  | 1           | 2.000000 |

Latency: 567 ns

Initially, all cache lines are set to a shared status. Following this, cache 0 successfully writes, transitioning to a modified state, which causes the states of the other caches to become invalid. 
Subsequently, when the remaining three caches attempt to read the data, they experience read misses. However, they are able to fetch the data from cache 0, which is in the modified state. This action shifts the three caches into shared states, while the cache 0, previously modified, moves to an owned state.
Finally, a cache in the shared state successfully performs a write operation, triggering the invalidation of all other cache lines, including the one in the owned state.

### Experienments
##### Performance Evaluation with MOESI Protocol
###### Processor = 1
1. fft_1024

| RHit   | RMiss | WHit  | WMiss | Hitrate  | MAccessTime | WaitBus  |
|--------|-------|-------|-------|----------|-------------|----------|
| 503437 | 2982  | 152997| 994   | 99.39795 | 4426        | 1.000226 |

Total access time: 4426 ns
Average bus time: 1.000226 ns
Simulation time latency: 1.768299 ms
Memory access rate: 0.002503 accesses/ns

2. matrix_vector_8_5000

| RHit   | RMiss | WHit  | WMiss | Hitrate  | MAccessTime | WaitBus  |
|--------|-------|-------|-------|----------|-------------|----------|
| 620045 | 20003 | 80020 | 12    | 97.22045 | 20025       | 1.000050 |

Total access time: 20025 ns
Average bus time: 1.000050 ns
Simulation time latency: 3.462698 ms
Memory access rate: 0.005783 accesses/ns

3. matrix_mul_50_50

| RHit   | RMiss | WHit  | WMiss | Hitrate  | MAccessTime | WaitBus  |
|--------|-------|-------|-------|----------|-------------|----------|
| 1631711| 939   | 255148| 2     | 99.95015 | 941         | 1.001063 |

Total access time: 941 ns
Average bus time: 1.001063 ns
Simulation time latency: 3.870644 ms
Memory access rate: 0.000243 accesses/ns


###### Processor = 4

1. fft_1024

| RHit   | RMiss | WHit  | WMiss | Hitrate  | MAccessTime | WaitBus  |
|--------|-------|-------|-------|----------|-------------|----------|
| 128090 | 861   | 39427 | 138   | 99.40718 | 757         | 1.017419 |
| 128158 | 913   | 39689 | 248   | 99.31305 | 860         | 1.019398 |
| 128088 | 920   | 39654 | 241   | 99.31262 | 876         | 1.039386 |
| 128136 | 895   | 39662 | 244   | 99.32578 | 838         | 1.039202 |

Total access time: 3331 ns
Average bus time: 1.02885125 ns
Simulation time latency: 0.427177 ms
Memory access rate: 0.007798 accesses/ns

2. matrix_vector_8_5000

| RHit   | RMiss | WHit  | WMiss | Hitrate  | MAccessTime | WaitBus  |
|--------|-------|-------|-------|----------|-------------|----------|
| 155009 | 5003  | 20004 | 4     | 97.21864 | 5008        | 1.048313 |
| 145665 | 14347 | 19673 | 336   | 91.84373 | 4812        | 1.024641 |
| 155008 | 5004  | 10466 | 9543  | 91.91928 | 4812        | 1.018655 |
| 155009 | 5003  | 20004 | 4     | 97.21864 | 5008        | 1.056099 |

Total access time: 19640 ns
Average bus time: 1.036927 ns
Simulation time latency: 0.866288 ms
Memory access rate: 0.022671 accesses/ns

3. matrix_mul_50_50

| RHit   | RMiss | WHit  | WMiss | Hitrate  | MAccessTime | WaitBus  |
|--------|-------|-------|-------|----------|-------------|----------|
| 424012 | 477   | 66337 | 2     | 99.90241 | 477         | 1.002083 |
| 391371 | 465   | 61234 | 2     | 99.89693 | 464         | 1.010684 |
| 424012 | 477   | 66337 | 2     | 99.90241 | 476         | 1.006250 |
| 391371 | 465   | 61234 | 2     | 99.89693 | 466         | 1.004283 |

Total access time: 1883 ns
Average bus time: 1.005825 ns
Simulation time latency: 1.029838 ms
Memory access rate: 0.001828 accesses/ns

##### Processor = 8

1. fft_1024

| RHit  | RMiss | WHit  | WMiss | Hitrate  | MAccessTime | WaitBus  |
|-------|-------|-------|-------|----------|-------------|----------|
| 65787 | 470   | 20518 | 68    | 99.38049 | 362         | 1.037356 |
| 65859 | 530   | 20790 | 181   | 99.18613 | 473         | 1.119870 |
| 65734 | 547   | 20714 | 183   | 99.16263 | 469         | 1.148494 |
| 65836 | 547   | 20778 | 191   | 99.15514 | 464         | 1.167170 |
| 65785 | 548   | 20740 | 192   | 99.15201 | 469         | 1.128413 |
| 65791 | 527   | 20747 | 175   | 99.19532 | 455         | 1.170576 |
| 65832 | 551   | 20781 | 182   | 99.16081 | 449         | 1.125259 |
| 65762 | 513   | 20719 | 171   | 99.21528 | 449         | 1.144605 |

Total access time: 3590 ns
Average bus time: 1.130218 ns
Simulation time latency: 0.223187 ms
Memory access rate: 0.016085 accesses/ns

2. matrix_vector_8_5000

| RHit  | RMiss | WHit  | WMiss | Hitrate  | MAccessTime | WaitBus  |
|-------|-------|-------|-------|----------|-------------|----------|
| 76764 | 3242  | 7374  | 2632  | 93.47420 | 2526        | 1.093862 |
| 76773 | 3233  | 7151  | 2864  | 93.22714 | 2521        | 1.136350 |
| 75392 | 4614  | 6344  | 3675  | 90.79256 | 2707        | 1.131123 |
| 75323 | 4683  | 6245  | 3784  | 90.59588 | 2463        | 1.151311 |
| 75356 | 4650  | 6238  | 3814  | 90.60161 | 2453        | 1.174125 |
| 75620 | 4386  | 6292  | 3799  | 90.91535 | 2457        | 1.192050 |
| 76741 | 3265  | 7615  | 2389  | 93.71848 | 2522        | 1.185982 |
| 76951 | 3055  | 7392  | 2627  | 93.68842 | 2517        | 1.214401 |

Total access time: 20166 ns
Average bus time: 1.159901 ns
Simulation time latency: 0.469789 ms
Memory access rate: 0.042926 accesses/ns

3. mult_50_50

| RHit   | RMiss | WHit  | WMiss | Hitrate  | MAccessTime | WaitBus  |
|--------|-------|-------|-------|----------|-------------|----------|
| 228170 | 401   | 35719 | 2     | 99.84752 | 401         | 1.024752 |
| 195529 | 389   | 30616 | 2     | 99.82740 | 390         | 1.028133 |
| 228168 | 403   | 35719 | 2     | 99.84676 | 402         | 1.029557 |
| 195529 | 389   | 30616 | 2     | 99.82740 | 389         | 1.035714 |
| 195529 | 389   | 30616 | 2     | 99.82740 | 390         | 1.035806 |
| 195529 | 389   | 30616 | 2     | 99.82740 | 389         | 1.035714 |
| 195529 | 389   | 30616 | 2     | 99.82740 | 390         | 1.033248 |
| 195529 | 389   | 30616 | 2     | 99.82740 | 391         | 1.033248 |

Total access time: 3142 ns
Average bus time: 1.032022 ns
Simulation time latency: 0.569209 ms
Memory access rate: 0.005520 accesses/ns

1. The performance of p1 has the same results of assignment 1 because p1 operates exclusively within the modified or exclusive states, avoiding the shared or owned states. This behavior aligns with the write-back policy (modified is equal to the dirty state), where data updates to the memory occur only during LRU (Least Recently Used) eviction events.
2. The MOESI protocol significantly outperforms the valid-invalid protocol in terms of latency. This is due to data-to-data transfers, where a read miss can retrieve data from other caches. This aspect is one of the key sources of performance enhancement with MOESI. 
3. The MOESI protocol continues to encounter issues with false sharing. A notable example is the 8_5000 test, where the involvement of 8 cores significantly impacts the hit rate negatively. Upon examining the addresses, it becomes evident that caches frequently write to the same cache lines, leading to numerous invalidations.
4. As the number of processors increases, the hit rate tends to drop. One reason is the increase in write probe invalidations. Additionally, with a greater number of processors, the chances that several caches will contain duplicates of the same data go up. To keep these caches coherent, more invalidation messages are necessary. Furthermore, the probability of false sharing increases alongside the processor numbers. With an increasing number of processors, both spatial and temporal locality are impacted, as cache lines might access disparate data sets, potentially leading to the invalidation of the states of other caches. 
5. Minimal bus contention implies that cache operations can be executed promptly. However, the data also indicates that as the number of processors grows, the average time spent on bus contention tends to increase, because more caches will access memory or send cache-to-cache data during the same period of time.
6. The memory access rate has risen due to a greater number of caches requesting data simultaneously within the same period.