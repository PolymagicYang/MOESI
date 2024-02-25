##### Bus Design
The Bus is implemented by FIFO with priority, the bus could receice 1. actions from the cache 2. actions from the memory, because the actions from memory have the higher priority than the cache, it needs the FIFO with priority to enable this, because if the cache occupies the bus out of the time order, the later cache may assume the other cache already enter some states, but in reality they're not.

upper edge: some caches send signals to the bus and the memory also send to the bus, suppose this process is sequential, the momery should be the first served.

time ordering? the cache request comes to the bus at time 0ns, but it failed to occupy the bus, then the next clock cycle, there is another cache request comes in, how to handle this order? Is the order based on the time? (the first comes should be served firstly). If this is the case, if the memory request (memory request has a higher priority) comes later, the first comes cache request should be servced later or not.

Synchronization mannar for Manager and Snoofing protocol:
The Manager utilizes the cache interfaces to communicate, in the same time, the cache may snoof actions from other caches, this will involve the conficts.
To solve this, I add the port read at the start of every read. The bus is designed like this: it will send the signals to all the caches at once, and wait all the caches complete the state transition, the requested cache will say it finished this action (readmiss/hit), essentially the protocol aims to coherent all the cache lines, so the cache read/write must wait until all the caches finished the cache transition.

If a cache writes, the bus failed to wait all the caches, the cache reads the same data, at this time, because some caches don't finish the cache transition, it will read the old data. (This will not happen, because the bus executes in the falling edge, the Manager executes on the raising edge, so when Manager calls cache.read()/write(), the state transition signal should alredy in the port).

Use sc_signal, because the request always change, it won't remain the same.