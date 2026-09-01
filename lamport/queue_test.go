package lamport

import "testing"

func TestQueueAddGetRemove(t *testing.T) {
	q := NewRequestQueue(4)

	if _, ok := q.Get(2); ok {
		t.Fatal("empty queue reported a request for P2")
	}
	if _, ok := q.Earliest(); ok {
		t.Fatal("empty queue reported an earliest request")
	}
	if q.Len() != 0 {
		t.Fatalf("empty queue has length %d", q.Len())
	}

	ts := Timestamp{Time: 5, Pid: 2}
	q.Add(ts)

	got, ok := q.Get(2)
	if !ok || got != ts {
		t.Fatalf("Get(2) = %v, %v; want %v, true", got, ok, ts)
	}
	if q.Len() != 1 {
		t.Fatalf("length %d after one Add", q.Len())
	}

	q.Remove(2)
	if _, ok := q.Get(2); ok {
		t.Fatal("P2 still present after Remove")
	}
	q.Remove(2) // removing twice must be harmless
	if q.Len() != 0 {
		t.Fatalf("length %d after Remove", q.Len())
	}
}

// Earliest must use the full total order, tiebreak included.
func TestQueueEarliestUsesTotalOrder(t *testing.T) {
	q := NewRequestQueue(4)
	q.Add(Timestamp{Time: 9, Pid: 0})
	q.Add(Timestamp{Time: 4, Pid: 3})
	q.Add(Timestamp{Time: 4, Pid: 1}) // same time as P3, lower id wins
	q.Add(Timestamp{Time: 7, Pid: 2})

	got, ok := q.Earliest()
	if !ok {
		t.Fatal("Earliest reported nothing")
	}
	if want := (Timestamp{Time: 4, Pid: 1}); got != want {
		t.Fatalf("Earliest = %v, want %v (queue %v)", got, want, q)
	}
}

// Condition (i) of rule 5.
func TestQueueIsEarliest(t *testing.T) {
	q := NewRequestQueue(4)

	if q.IsEarliest(1) {
		t.Fatal("a process with no request cannot be earliest")
	}

	q.Add(Timestamp{Time: 4, Pid: 1})
	if !q.IsEarliest(1) {
		t.Fatal("sole request must be earliest")
	}

	q.Add(Timestamp{Time: 9, Pid: 0})
	if !q.IsEarliest(1) {
		t.Fatalf("P1 at 4 should still beat P0 at 9 (queue %v)", q)
	}
	if q.IsEarliest(0) {
		t.Fatalf("P0 at 9 must not be earliest (queue %v)", q)
	}

	// A tie decided by process id.
	q.Add(Timestamp{Time: 4, Pid: 0})
	if q.IsEarliest(1) {
		t.Fatalf("P0 wins the tie at time 4, so P1 is not earliest (queue %v)", q)
	}
	if !q.IsEarliest(0) {
		t.Fatalf("P0 must be earliest after winning the tie (queue %v)", q)
	}

	// Rule 4: once the winner's request is removed, the next one takes over.
	q.Remove(0)
	if !q.IsEarliest(1) {
		t.Fatalf("P1 must be earliest after P0 released (queue %v)", q)
	}
}

// Exactly one process can be earliest at a time. This is the queue-level shape
// of mutual exclusion: if two processes could both read themselves as first in
// the same queue state, rule 5 would grant twice.
func TestQueueHasExactlyOneEarliest(t *testing.T) {
	q := NewRequestQueue(5)
	q.Add(Timestamp{Time: 3, Pid: 4})
	q.Add(Timestamp{Time: 3, Pid: 2})
	q.Add(Timestamp{Time: 8, Pid: 0})
	q.Add(Timestamp{Time: 3, Pid: 1})

	count := 0
	for pid := range ProcessID(5) {
		if q.IsEarliest(pid) {
			count++
		}
	}
	if count != 1 {
		t.Fatalf("%d processes read themselves as earliest in %v, want exactly 1", count, q)
	}
}

// Only reachable when in-order delivery is violated; it must not corrupt state.
func TestQueueAddReplaces(t *testing.T) {
	q := NewRequestQueue(3)
	q.Add(Timestamp{Time: 2, Pid: 1})
	q.Add(Timestamp{Time: 6, Pid: 1})

	if q.Len() != 1 {
		t.Fatalf("length %d after re-adding the same process, want 1", q.Len())
	}
	got, _ := q.Get(1)
	if want := (Timestamp{Time: 6, Pid: 1}); got != want {
		t.Fatalf("Get(1) = %v, want %v", got, want)
	}
}
