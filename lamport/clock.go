// Package lamport implements the distributed mutual exclusion algorithm from
// Leslie Lamport's 1978 paper "Time, Clocks, and the Ordering of Events in a
// Distributed System" (CACM 21(7), pp. 558-565).
//
// The package is meant to be read next to the paper.
package lamport

import "fmt"

// ProcessID identifies one process in the system.
//
// The paper's total order on events breaks ties between equal clock values with
// an arbitrary but fixed total order on processes, written as the relation "≺".
// We use the numeric order of these IDs. Any order works as long as every process
// uses the same one.
type ProcessID int

// Beats reports whether p wins a tie against other in the process order ≺.
func (p ProcessID) Beats(other ProcessID) bool { return p < other }

// Timestamp is a point in logical time tagged with the process that produced
// it.
//
// The clock condition alone yields only a partial order. Every process has to rank
// the pending requests using only local state and independently arrive at the same
// answer, and a partial order can leave two requests incomparable, with no way for
// two processes to agree on which one wins.
//
// Pairing the counter with the originating process ID extends the partial order
// into a total one.
type Timestamp struct {
	Time int
	Pid  ProcessID
}

// Less reports whether t is ordered strictly before other in the total order
// the paper writes as "⇒".
func (t Timestamp) Less(other Timestamp) bool {
	if t.Time != other.Time {
		return t.Time < other.Time
	}
	return t.Pid.Beats(other.Pid)
}

// SatisfiedBy checks one peer against condition (ii) of rule 5.
//
// Rule 5(ii) says a process may take the resource only once it has received a
// message timestamped later than its own request t from every other process.
// This method answers that for a single peer; the caller runs it against each
// peer in turn.
//
// u is the highest logical time this process has seen in any message from peer.
// The result says whether u is late enough to rule out peer still holding a
// request that would come before t.
func (t Timestamp) SatisfiedBy(u int, peer ProcessID) bool {
	if t.Pid.Beats(peer) {
		return u >= t.Time
	}
	return u > t.Time
}

func (t Timestamp) String() string { return fmt.Sprintf("%d:P%d", t.Time, t.Pid) }

// Clock is a Lamport logical clock.
type Clock struct {
	pid  ProcessID
	time int
}

// NewClock returns a clock for process pid, started at logical time zero.
func NewClock(pid ProcessID) *Clock { return &Clock{pid: pid} }

// Event is called when an event happens and the clock needs to advance.
func (c *Clock) Event() Timestamp {
	c.time++
	return Timestamp{Time: c.time, Pid: c.pid}
}

// Receive is called when the process's goroutine pulls the message from its
// channel and starts acting on it (this is what we consider receiving a message).
func (c *Clock) Receive(sent Timestamp) Timestamp {
	c.time = max(c.time, sent.Time) + 1
	return Timestamp{Time: c.time, Pid: c.pid}
}

// Now returns the current logical time without advancing it.
func (c *Clock) Now() Timestamp { return Timestamp{Time: c.time, Pid: c.pid} }
