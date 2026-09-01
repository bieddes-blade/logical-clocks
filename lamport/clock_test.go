package lamport

import "testing"

// IR1: the clock advances between any two successive events, so timestamps at a
// single process strictly increase and never repeat.
func TestEventIsStrictlyIncreasing(t *testing.T) {
	c := NewClock(3)
	prev := c.Now()
	for i := range 100 {
		got := c.Event()
		if !prev.Less(got) {
			t.Fatalf("event %d: %v is not ordered after %v", i, got, prev)
		}
		prev = got
	}
}

// IR2(b): on receiving a message, a process sets its clock above both its own
// current value and the sender's timestamp. Going above its own value keeps the
// process's events strictly increasing; going above the sender's is what makes
// C(send) < C(receive), which carries the ordering across the network.
func TestReceiveSatisfiesClockCondition(t *testing.T) {
	for _, tc := range []struct {
		name        string
		local, sent int
	}{
		{"sender ahead", 2, 10},
		{"sender behind", 10, 2},
		{"equal", 5, 5},
		{"from zero", 0, 0},
	} {
		t.Run(tc.name, func(t *testing.T) {
			c := NewClock(1)
			c.time = tc.local
			sent := Timestamp{Time: tc.sent, Pid: 0}

			got := c.Receive(sent)

			if got.Time <= tc.local {
				t.Errorf("receive returned %d, must exceed local clock %d", got.Time, tc.local)
			}
			if got.Time <= sent.Time {
				t.Errorf("receive returned %d, must exceed sender stamp %d", got.Time, sent.Time)
			}
			if c.Now() != got {
				t.Errorf("clock is at %v but receive reported %v", c.Now(), got)
			}
		})
	}
}

// Less must be a strict total order. Without totality two processes could hold
// two requests neither can rank, and rule 5 would have no one to grant to.
func TestTimestampIsStrictTotalOrder(t *testing.T) {
	var all []Timestamp
	for time := range 5 {
		for pid := range ProcessID(5) {
			all = append(all, Timestamp{Time: time, Pid: pid})
		}
	}
	for _, a := range all {
		if a.Less(a) {
			t.Errorf("%v is ordered before itself", a)
		}
		for _, b := range all {
			ab, ba := a.Less(b), b.Less(a)
			if a == b {
				if ab || ba {
					t.Errorf("%v and %v are equal but comparable", a, b)
				}
				continue
			}
			if ab == ba {
				t.Errorf("%v vs %v: exactly one must come first, got %v/%v", a, b, ab, ba)
			}
			for _, c := range all {
				if a.Less(b) && b.Less(c) && !a.Less(c) {
					t.Errorf("not transitive: %v < %v < %v but not %v < %v", a, b, c, a, c)
				}
			}
		}
	}
}

// The property the clocks exist to provide: if a happened before b, C(a) < C(b).
func TestHappenedBeforeImpliesClockOrder(t *testing.T) {
	p0, p1 := NewClock(0), NewClock(1)

	p0.Event()
	p0.Event()
	send := p0.Event()

	recv := p1.Receive(send)
	if !send.Less(recv) {
		t.Fatalf("send %v not ordered before its receive %v", send, recv)
	}

	reply := p1.Event()
	if !recv.Less(reply) || !send.Less(reply) {
		t.Fatalf("reply %v must follow both %v and %v", reply, recv, send)
	}

	back := p0.Receive(reply)
	if !reply.Less(back) {
		t.Fatalf("reply %v not ordered before its receive %v", reply, back)
	}
}

// Once a process has heard time u from a peer, it holds every request that peer
// made before u, so the requests it might still be missing are those at time >= u.
// The condition holds only if every one of those would be ordered after the process's
// own request.
func TestSatisfiedByMatchesItsMeaning(t *testing.T) {
	const horizon = 25
	for time := 1; time <= 12; time++ {
		for pid := range ProcessID(4) {
			req := Timestamp{Time: time, Pid: pid}
			for peer := range ProcessID(4) {
				if peer == pid {
					continue
				}
				for u := 0; u <= 15; u++ {
					safe := true
					for ts := u; ts <= horizon; ts++ {
						if (Timestamp{Time: ts, Pid: peer}).Less(req) {
							safe = false
							break
						}
					}
					if got := req.SatisfiedBy(u, peer); got != safe {
						t.Fatalf("req %v, peer P%d, heard %d: SatisfiedBy = %v, but a missing request could beat it = %v",
							req, peer, u, got, !safe)
					}
				}
			}
		}
	}
}
