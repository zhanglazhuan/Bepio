# Messages host tests

```powershell
python tests/unittest/messages/run.py
```

Requires Python and GCC (`CC` may specify another compatible compiler executable).
Compiles the actual messaging service, Messages MVC and shell, app_manager, Settings
and Contacts sources. Capacities are reduced to three messages per box for boundary tests.

Coverage includes canonical UTF-8 and exact 256-byte boundaries; full mailboxes without
eviction; unread/read state; reverse insertion order; retained and recently deleted
receive deduplication; offline queueing; server ACK state transitions; stale attempt
rejection; stable retry IDs; explicit simulated results; immutable submitted content;
save/edit/discard; send/delete/leave confirmations; editor retention on full mailbox and
forced app closure; reply and inactive Contacts lookup; recipient snapshots after contact
deletion; shell handler validation and the original single-app ownership contract.

Explicit startup, logging/mutex and console output have host adapters shared with existing tests.
The MinGW-only missing errno fallback lives in host/errno_compat.h. Mutexes use single-threaded recursion counters;
these tests do not verify real scheduling, shell UART tokenization, target stack usage,
real cellular transmission, server interoperability, persistent storage or hardware.
