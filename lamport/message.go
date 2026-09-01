package lamport

import "fmt"

// Kind is the type of a message. The paper names three.
type Kind uint8

const (
	// Request is "Tm:Pi requests resource", broadcast by rule 1.
	Request Kind = iota
	// Release is "Pi releases resource", broadcast by rule 3.
	Release
	// Ack is the acknowledgment rule 2 sends back to a requester.
	//
	// An acknowledgment carries no information beyond its own timestamp. It
	// exists for one reason: to make condition (ii) of rule 5 satisfiable. A
	// silent process never sends the requester anything later than Tm, so
	// without it a request could wait forever and condition III would fail.
	//
	// Because that is its only job, rule 2's footnote allows it to be skipped
	// whenever the responder has already sent the requester a late enough
	// message. The obligation is discharged either way.
	Ack
)

func (k Kind) String() string {
	switch k {
	case Request:
		return "REQUEST"
	case Release:
		return "RELEASE"
	case Ack:
		return "ACK"
	}
	return fmt.Sprintf("Kind(%d)", uint8(k))
}

// Message is one message in flight between two processes.
//
// Sent is the timestamp of the event that sent the message, which is rule
// IR2(a). Two things follow from the "one rule, one event" convention:
//
// For a Request, Sent is exactly the Tm that names the request. The request and
// its timestamp are the same thing, so nothing else is needed to identify it.
//
// For an Ack, Sent is the timestamp of the receive event that triggered it,
// because rule 2 makes receiving the request and replying a single event.
//
// A Release deliberately carries no Tm. Rule 4 says to remove any request from
// that process, and a process cannot request again until it has released, so at
// most one of its requests is ever pending and "which one" cannot be ambiguous.
type Message struct {
	Kind Kind
	From ProcessID
	Sent Timestamp
}

func (m Message) String() string {
	return fmt.Sprintf("%s from P%d at %s", m.Kind, m.From, m.Sent)
}
