# Network Architecture

## Protocol Machine

```AsciiDoc
          PM (Layer N+1)

                |
                |                PDU:
         -------------------
         |-----------------|     -------------------------      -----------------------
OS  <--> || State Machine ||  <- | PCI | SDU (User Data) | ->   | Remote PM (Layer N) |    
         |-----------------|     -------------------------      -----------------------
         |                 |
         |-----------------|
         ||  State Vector ||
         |-----------------|
         -------------------         
                |
                |
               
          PM (Layer N-1)
```

## Association, Flow, Connection, Binding

* An association represents the minimal shared state
  minimal coupling, often associated with connectionless communication.
* A flow has more shared state but not tightly coupled
  (no feedback), as found in some network protocols.
* A connection has a more tightly coupled shared state
  (with feedback), as with so-called end-to-end transport protocols.
* A binding has the most tightly coupled shared state,
  generally characterized by shared memory.


## Mechanism and Policy

The mechanism are reflected in the PCI (Check Sum Field).
A policy determines what kind of checksum, with what init vector, and how often it is calculated.

### Voice Protocol Allowing small Gaps (Implemented with Policy)

It had always been thought that another transport protocol would be required for voice. 
With voice, the PDUs must be ordered, but short gaps in the data stream can be tolerated.
So, it was thought that a new protocol would be required that allowed for
small gaps (instead of the protocol just retransmitting everything). However, a
new protocol is not required; all that is necessary is to modify the acknowledgment
policy…and lie. There is no requirement to tell the truth! If the gap is
short, send an ack anyway, even though not all the data has been received.
There is no requirement in any existing transport protocols to tell the truth!

## Two fundamental Data Transfer Protocols

### Relaying- and Multiplexing Protocols

The MAC does relaying and multiplexing

### Error- and Flow-control Protocols

The data link layer does “end-to-end” error control.




