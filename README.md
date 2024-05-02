# logical-clocks

## Problem statement

This algorithm solves a mutual exclusion problem using an approach from [Time, Clocks, and the Ordering of Events in a Distributed System](https://lamport.azurewebsites.net/pubs/time-clocks.pdf), a paper by Leslie Lamport.

Consider a system composed of a fixed collection of processes which share a single resource. (In this implementation, the processes are represented by threads, and the resource is represented by an integer variable that threads can attempt to write into simultaneously.) Only one process should use the resource at a time, so the processes must synchronize themselves to avoid conflict.

We wish to find an algorithm for granting the resource to a process which satisfies the following three conditions:

1. A process which has been granted the resource must release it before it can be granted to another process.
2. Different requests for the resource must be granted in the order in which they are made.
3. If every process which is granted the resource eventually releases it, then every request is eventually granted.

We assume that for any two processes P_i and P_j, the messages sent from P_i to P_j are received in the same order as they are sent. Moreover, we assume that every message is eventually received. In the future, I will add a message backlog to this implementation to avoid the need for the following assumptions.

## How to run it?

```
g++ -std=c++17 threads.cpp -o threads
./threads
```

## How to read the output?

The threads send three types of messages to each other: REQUEST, RELEASE and ACK. The last number in each event description is the timestamp of that event. These timestamps and a rule for breaking ties are enough to create a total ordering of events in the system. Each thread maintains its own logical clock and updates its value after certain events, such as sending all other threads a message (```++clock```) or receiving a message from another thread (```clock = max(clock + 1, message.timestamp + 1)```).

ACK messages are sent in response to REQUEST messages, so that the sender thread would be able to know when all other threads have received its resource request.

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

The variable ```NUM_THREADS``` refers to the number of processes competing for the resource; the variable ```NUM_WRITES``` refers to the number of times these processes will try to update the value of the resource. The last line of the output should be a number equal to ```NUM_THREADS * NUM_WRITES```. In case of a race condition, a warning will be printed into the output.

## Acknowledgements

The message queue is based on a thread-safe queue class from this repository: https://github.com/K-Adam/SafeQueue/tree/master
