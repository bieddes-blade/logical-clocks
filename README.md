# logical-clocks

This algorithm solves a mutual exclusion problem using an approach from [Time, Clocks, and the Ordering of Events in a Distributed System](https://lamport.azurewebsites.net/pubs/time-clocks.pdf), a paper by Leslie Lamport.

## What does it do?

```
Thread 0 sent REQUEST to 2 at 0
Thread 2 sent REQUEST to 1 at 0
Thread 0 received REQUEST from 1 at 2
Thread 2 received REQUEST from 0 at 2
...
Thread 0 received ACK from 1 at 5
  Resource was granted to thread 0 at 5
  Resource was released from thread 0 at 6
Thread 0 sent RELEASE to 1 at 6
Thread 1 received RELEASE from 0 at 7
```

## How to run it?

```
g++ -std=c++17 threads.cpp -o threads
```

## Algorithm

Time, Clocks, and the Ordering of Events in a Distributed System https://lamport.azurewebsites.net/pubs/time-clocks.pdf

Used a safe queue class from this repository https://github.com/K-Adam/SafeQueue/tree/master
