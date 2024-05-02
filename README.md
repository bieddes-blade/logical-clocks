# logical-clocks

## What does it do?

```
Thread 0 received ACK from 1 at 4
Thread 0 received ACK from 2 at 5
  Resource was granted to thread 0 at 5
  Resource was released from thread 0 at 6
Thread 1 received RELEASE from 0 at 7
  Resource was granted to thread 1 at 7
Thread 0 sent RELEASE to 1 at 6
Thread 0 sent RELEASE to 2 at 6
Thread 2 received RELEASE from 0 at 7
  Resource was released from thread 1 at 8
```

## How to run it?

```
g++ -std=c++17 threads.cpp -o threads
```

## Algorithm

Time, Clocks, and the Ordering of Events in a Distributed System https://lamport.azurewebsites.net/pubs/time-clocks.pdf

Used a safe queue class from this repository https://github.com/K-Adam/SafeQueue/tree/master
