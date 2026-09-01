package lamport

import "fmt"

// Kind is the type of a message: request, release, or ack.
type Kind uint8

const (
	// Request is "Tm:Pi requests resource", broadcast by rule 1.
	Request Kind = iota
	// Release is "Pi releases resource", broadcast by rule 3.
	Release
	// Ack is the acknowledgment rule 2 sends back to a requester.
	//
	// An acknowledgment carries no information beyond its own timestamp. It
	// exists to make condition (ii) of rule 5 satisfiable. A silent process
	// never sends the requester anything later than Tm, so without it a
	// request could wait forever and condition III would fail.
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

// Message has a Sent timestamp field.
//
// For a Request, Sent is the Tm that identifies the request.
//
// For an Ack, Sent is the timestamp of the receive that triggered it. A process
// advances its clock once when it takes a request off its inbox, then stamps the
// acknowledgment it sends back with that same value instead of advancing again.
//
// A Release carries no request timestamp. A process receiving one removes
// whatever request it is currently holding from that sender. There is never
// more than one to choose between, because a process cannot request the resource
// again until it has released it.
type Message struct {
	Kind Kind
	From ProcessID
	Sent Timestamp
}

func (m Message) String() string {
	return fmt.Sprintf("%s from P%d at %s", m.Kind, m.From, m.Sent)
}
