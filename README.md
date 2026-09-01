# logical-clocks

Leslie Lamport's distributed mutual exclusion algorithm in Go, from [Time, Clocks, and the Ordering of Events in a Distributed System](https://lamport.azurewebsites.net/pubs/time-clocks.pdf) (CACM 21(7), 1978).

## The problem

Consider a system composed of a fixed collection of processes which share a single resource. Only one process should use the resource at a time, so the processes must synchronize themselves to avoid conflict.

In this implementation, the processes are represented by goroutines that communicate only by message passing. The resource they contend for is a plain `int`, incremented by the goroutines without a mutex or an atomic. With only the algorithm protecting it, a break in mutual exclusion means two goroutines writing the same variable at once, and `go test -race` reports that.

A correct solution satisfies three conditions:

1. A process granted the resource must release it before it can be granted to another.
2. Different requests must be granted in the order in which they are made.
3. If every process granted the resource eventually releases it, then every request is eventually granted.

## Logical clocks

Each process keeps a counter and follows two rules:

-**IR1** - increment the counter between any two successive events.
-**IR2** - a sent message carries the sender's counter value; on receiving a message stamped `Tm`, set the counter above both its current value and `Tm`.

This gives the clock condition: if `a` happened before `b`, then `C(a) < C(b)`.

Counters alone yield only a *partial* order, since two events at different processes can share a value. Pairing each counter with its process ID and breaking ties on that ID extends it to a total order.

## The algorithm

Every process keeps a **request queue** that no other process ever sees. It is a local replica of the requests that process believes are pending, and the five rules keep the replicas consistent enough that all of them agree on which request is earliest.

1. **Request** - broadcast `Tm:Pi requests` to every process, and add it to your own queue.
2. **On receiving a request** - add it to your queue and send back a timestamped acknowledgment.
3. **Release** - remove your own request from your queue, broadcast `Pi releases`.
4. **On receiving a release** - remove that process's request from your queue.
5. **Grant yourself the resource** when both hold:
   -**(i)** your request is ordered before every other request in your queue, and
   -**(ii)** you have received a message from every other process timestamped later than `Tm`.

### Why condition (ii) works

Condition (i) alone only says that a process is first among the requests it knows about. Condition (ii) is what adds that it knows about all of them:

> Suppose a peer made a request stamped earlier than this one. By rule 1 the peer broadcast it here. By IR1, every message the peer sends afterwards carries a higher timestamp. Condition (ii) says a message from that peer stamped later than this request has arrived - so it was sent *after* the peer's request was. **Because messages between a pair arrive in the order they were sent, the peer's request must have arrived first.** So it is already in the queue, and condition (i) accounted for it.

The acknowledgments in rule 2 exist only to make condition (ii) satisfiable - a silent peer would never send anything later, and a request would wait forever. That is also why an acknowledgment can be skipped whenever the responder has already sent the requester a late enough message: the obligation is met either way.

## Assumptions

The paper assumes, and so does this implementation:

1. Messages between any pair of processes arrive in the order they were sent.
2. Every message is eventually received.
3. Every process can send directly to every other.

## Layout

```
lamport/clock.go     logical clocks, timestamps, and the total order
lamport/message.go   the three message types
lamport/queue.go     the request queue
```

## Running the tests

```
go test ./...
```

A failure of mutual exclusion shows up as two unsynchronized writes to the shared counter, so the race detector catches it:

```
go test ./... -race
```

On Windows `-race` requires cgo and a C compiler on `PATH` (MinGW-w64 or similar). Without one it will not build, though `go test` on its own still runs every test.

## Status

The clocks, message types, and request queue are implemented and tested. The network, the process loop, and a runnable demo are still to come.
