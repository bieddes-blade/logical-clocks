package lamport

import (
	"slices"
	"strings"
)

// entry is one slot of a RequestQueue: the request that process has pending, if
// it has one. ts is meaningful only while pending is true.
type entry struct {
	ts      Timestamp
	pending bool
}

// RequestQueue is one process's private view of the requests it believes are
// currently pending, ordered by the total order on timestamps.
//
// The representation is a flat array indexed by process ID rather than a heap
// or a sorted list, because of one bounding fact: a process cannot request the
// resource again until it has released it, so the queue holds at most one entry
// per process and can never exceed N. At that size a linear scan over
// contiguous memory beats a tree or a heap outright, and it allocates nothing
// after construction.
type RequestQueue struct {
	entries []entry
}

// NewRequestQueue returns an empty queue sized for n processes.
func NewRequestQueue(n int) *RequestQueue {
	return &RequestQueue{entries: make([]entry, n)}
}

// Add records a pending request. Used by rule 1 for a process's own request and
// by rule 2 for a request received from a peer.
//
// Add replaces any existing entry for the same process. Under the paper's
// in-order delivery assumption that case never arises, since a release always
// reaches a peer before the next request from the same sender. It can happen
// only when in-order delivery is violated, which is exactly the failure the
// out-of-order network is built to demonstrate.
func (q *RequestQueue) Add(ts Timestamp) {
	q.entries[ts.Pid] = entry{ts: ts, pending: true}
}

// Remove drops pid's pending request, if it has one. Used by rule 3 for a
// process's own request and by rule 4 on receiving a release. It is a no-op
// when there is nothing to remove.
func (q *RequestQueue) Remove(pid ProcessID) {
	q.entries[pid] = entry{}
}

// Get returns pid's pending request.
func (q *RequestQueue) Get(pid ProcessID) (Timestamp, bool) {
	e := q.entries[pid]
	return e.ts, e.pending
}

// Earliest returns the request ordered first under the total order, and whether
// the queue holds any request at all.
func (q *RequestQueue) Earliest() (Timestamp, bool) {
	var best Timestamp
	found := false
	for _, e := range q.entries {
		if !e.pending {
			continue
		}
		if !found || e.ts.Less(best) {
			best, found = e.ts, true
		}
	}
	return best, found
}

// IsEarliest reports condition (i) of rule 5 for pid: pid has a request in this
// queue, and it is ordered before every other request here.
//
// This is only half of the grant test. On its own it says that pid is first
// among the requests this process knows about; condition (ii) is what upgrades
// that to knowing about all of them.
func (q *RequestQueue) IsEarliest(pid ProcessID) bool {
	own := q.entries[pid]
	if !own.pending {
		return false
	}
	for i, e := range q.entries {
		if !e.pending || ProcessID(i) == pid {
			continue
		}
		if e.ts.Less(own.ts) {
			return false
		}
	}
	return true
}

// Len returns the number of pending requests.
func (q *RequestQueue) Len() int {
	n := 0
	for _, e := range q.entries {
		if e.pending {
			n++
		}
	}
	return n
}

// String renders the queue in order, for tracing.
func (q *RequestQueue) String() string {
	pending := make([]Timestamp, 0, len(q.entries))
	for _, e := range q.entries {
		if e.pending {
			pending = append(pending, e.ts)
		}
	}
	slices.SortFunc(pending, func(a, b Timestamp) int {
		switch {
		case a.Less(b):
			return -1
		case b.Less(a):
			return 1
		default:
			return 0
		}
	})

	parts := make([]string, len(pending))
	for i, ts := range pending {
		parts[i] = ts.String()
	}
	return "[" + strings.Join(parts, " ") + "]"
}
