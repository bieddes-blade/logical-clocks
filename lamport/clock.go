// Package lamport implements the distributed mutual exclusion algorithm from
// Leslie Lamport's 1978 paper "Time, Clocks, and the Ordering of Events in a
// Distributed System" (CACM 21(7), pp. 558-565).
//
// The package is meant to be read next to the paper. Where the code implements
// a numbered rule, the comment names it.
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

// SatisfiedBy reports whether a message from peer carrying logical time u is
// late enough to discharge peer's share of condition (ii) of rule 5, for a
// request stamped t.
//
// What condition (ii) guards against is an earlier request from a peer that has
// not arrived yet. Condition (i) only says t is first among the requests this
// process already holds. If it takes the resource and that earlier request turns
// up afterwards, requests were not granted in the order they were made.
//
// What a message from peer tells the process: IR1 makes peer's own events
// strictly increasing, so every event at peer before this message carries a time
// below u. If peer had requested the resource below u, that request came first,
// and rule 1 says peer broadcast it to everyone. Since messages between a pair
// arrive in the order they were sent, that request arrived here first. So this
// process already holds every request peer made below u, and the only ones it
// could still be missing are stamped u or later. Hearing something recent from a
// peer stands in for knowing everything old from it.
//
// The earliest request peer could still be hiding is one stamped exactly u. Let's
// check whether a request from peer stamped u loses to t. If it does, every later
// one does too, and nothing peer is still hiding can come before t.
//
// Unpacking that comparison gives the two cases:
//
//   - If t.Pid beats peer, a peer request at exactly t.Time loses to t, so
//     u == t.Time is already enough.
//   - Otherwise peer wins ties, so a peer request at exactly t.Time would win.
//     Ruling it out needs u strictly above t.Time.
//
// The paper states rule 5(ii) as strictly later for every peer, which is also
// correct, just more conservative: it makes a process keep waiting on a peer it
// already beats on the tiebreak.
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
