# logical-clocks

## Problem statement

This algorithm solves a mutual exclusion problem using an approach from [Time, Clocks, and the Ordering of Events in a Distributed System](https://lamport.azurewebsites.net/pubs/time-clocks.pdf), a paper by Leslie Lamport.

Consider a system composed of a fixed collection of processes which share a single resource. (In this implementation, the processes are represented by threads, and the resource is represented by an integer variable that threads can attempt to write into simultaneously.) Only one process should use the resource at a time, so the processes must synchronize themselves to avoid conflict.

We wish to find an algorithm for granting the resource to a process which satisfies the following three conditions:

1. A process which has been granted the resource must release it before it can be granted to another process.
2. Different requests for the resource must be granted in the order in which they are made.
3. If every process which is granted the resource eventually releases it, then every request is eventually granted.

## How to run it?

```
g++ -std=c++17 threads.cpp -o threads
```

## How to read the output?

Used a safe queue class from this repository https://github.com/K-Adam/SafeQueue/tree/master

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
