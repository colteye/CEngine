# CEngine Target Architecture: Networking

> **Status: consolidated reference.** Complete long-term client/server transport, replication, prediction, and interpolation contracts.
> `STATUS.md` and `IMPLEMENTATION.md` determine what is active; this file
> preserves detailed target information without expanding current scope.

## Contents

- [Networking and Replication](NETWORK.md#networking-and-replication)

## Networking and Replication

> **Status: detailed target contract.** Remote transport begins only at M2 in
> `IMPLEMENTATION.md`; full snapshots precede measured promotion of deltas,
> relevance, and prioritization.

### 1. Purpose

This document defines network transport, connections, authoritative replication,
entity relevance, user commands, client prediction, reconciliation,
interpolation, and network identity.

Networking is divided into mechanism and game meaning:

- `NetworkSystem` owns transport mechanics.
- `GameServer` and `GameClient` own replication and prediction policy.

### 2. Authority model

The server is authoritative for:

- entity existence;
- game rules and match outcomes;
- damage, inventory, and interaction results;
- authoritative transforms and physics;
- possession and spawning;
- scene selection and activation;
- replicated state.

The client sends intent and predicts only declared behavior. Client-reported
positions, hits, inventory changes, and spawn results are not trusted.

### 3. NetworkSystem responsibility

`NetworkSystem` owns:

- socket or platform transport adapters;
- connection establishment and teardown;
- packet send and receive;
- sequencing and acknowledgement;
- reliable and unreliable channels;
- fragmentation and reassembly;
- encryption/authentication hooks;
- congestion and bandwidth accounting;
- timeout and connection health;
- packet-level diagnostics.

It does not know what a player, weapon, entity property, scene, or game event
means.

The public transport can also create an in-process connection pair used by
single-player and listen servers.

The transport exposes three semantics, not a general matrix of channel
options:

| Stream | Delivery | Typical content |
| --- | --- | --- |
| control | reliable, ordered, bounded | connect and baseline requests |
| command/snapshot | unreliable, sequenced, redundancy allowed | user commands, simulation snapshots |
| game message | reliable, ordered by lane, bounded | client requests/results and rare reliable events |

Large content is delivered by the asset/content path, not tunneled through
gameplay messages. The in-process transport preserves the same message ordering,
validation, capacity, and ownership rules, though it may avoid byte encoding.

### 4. Protocol negotiation

Connection setup validates:

- engine network protocol version;
- game protocol version;
- network class and property schema version;
- required content manifest and scene revisions;
- enabled feature flags;
- authentication and session policy.

Compatibility failures produce a precise reason. The system never attempts to
interpret a packet using an incompatible layout.

Stable numeric IDs are generated deterministically during the game/content
build. Runtime registration order does not determine protocol identity.

### 5. User commands

The client samples input into a tick-addressed `UserCommand`:

```cpp
namespace CEngine::Game {

struct UserCommand {
    SimulationTick tick{};
    CommandSequence sequence{};
    QuantizedLook look{};
    QuantizedMove move{};
    ActionBits actions{};
};

} // namespace CEngine::Game
```

The exact fields belong to `GameShared`. Commands are compact, deterministic,
bounded, and validated by the server.

The client can send multiple recent commands in one packet to tolerate packet
loss. The server processes each accepted command at most once and applies limits
for command rate, time drift, movement magnitude, and action validity.

Ticks and sequences use modular comparison helpers with a protocol-defined
window; ordinary integer comparison is forbidden across wrap. A command older
than retained server history is rejected. A command too far in the future is
held only within a small bound or rejected. When a command for a tick is missing,
the server repeats the last continuous/held values for at most the negotiated
`max_command_hold_ticks`, clears edge/impulse fields, and then uses neutral
input. `GameShared` classifies every command field as continuous, held, or
impulse so packet loss can never repeat a one-shot action accidentally.

#### 5.1 Discrete client requests

`UserCommand` is for tick-rate simulation intent. Discrete actions that must
survive packet loss but are not replayed movement—selecting a dialogue option,
buying an item, changing a loadout, requesting a respawn, or operating a build
menu—use a versioned `ClientRequest` defined by `GameShared`.

```cpp
namespace CEngine::Game {

struct ClientRequestHeader {
    RequestSequence sequence{};
    SimulationTick observed_server_tick{};
    ClientRequestTypeId type{};
    std::uint16_t payload_size = 0;
};

} // namespace CEngine::Game
```

The payload has a registered bounded schema. It carries semantic IDs and values,
never a function name, pointer, local `EntityId`, or arbitrary object graph.
This is not a general RPC system.

The server processes requests during ingress in connection sequence order. It
validates schema, rate, player state, ownership, target `NetEntityId`, range,
cost, and game-specific permission before applying an outcome. A bounded
per-connection sequence window makes the operation at-most-once: duplicates
resend the cached result or are ignored, but never repeat side effects.

A `ClientRequestResult` reports acceptance or a stable rejection reason for UI
feedback. Durable consequences still appear in replicated state; the result is
not a second authority channel. Requests that belong to immediate fixed-tick
gameplay remain `UserCommand` fields so prediction and replay see them.

### 6. Network entity identity

The server assigns `NetEntityId` when a replicated entity enters the networked
simulation. It remains stable for that entity's replicated lifetime and has reuse
protection appropriate to the protocol.

Each connection maintains mappings:

```text
Server: NetEntityId -> server EntityId
Client: NetEntityId -> client EntityId
```

Network references use `NetEntityId`, never a local `EntityId` or memory address.
Reference application is deferred when the target has not yet been created in
the same snapshot stream.

`NetEntityId` contains or is paired with reuse generation/epoch information. A
late destroy or property delta can never target a different entity that reused
the same numeric slot.

### 7. Network classes and properties

A `NetworkClassDescriptor` defines:

- stable network class ID;
- server and client representation mapping;
- schema version;
- replicated properties and quantization;
- initial-state requirements;
- update priority and frequency policy;
- prediction participation;
- interpolation policy;
- owner-only or audience restrictions.

Scene serialization and replication metadata are distinct. A property can
participate in either or both; one mechanism does not imply the other.

Examples:

| Property | Scene | Replicate |
| --- | --- | --- |
| target name | yes | usually no |
| health | initial/default | yes |
| render handle | no | no |
| current transform | initial | yes |

### 8. Entity creation and destruction

When an entity becomes relevant, the server sends a create record containing:

- `NetEntityId`;
- network class ID and schema version;
- scene-instance or ownership context when required;
- initial replicated state;
- references that can be resolved in the baseline.

The client stages construction, validates the class and state, creates the local
representation, and publishes the mapping atomically.

Destruction contains `NetEntityId` and an optional reason. The client removes
presentation bindings and invalidates the mapping through normal deferred entity
destruction.

### 9. Snapshot replication

After a server tick, game replication captures a stable authoritative view. For
each connection it:

1. computes relevant entities;
2. prioritizes them within the connection's bandwidth budget;
3. compares current state with the acknowledged baseline;
4. emits entity creates, destroys, and property deltas;
5. includes server tick and acknowledgement metadata;
6. records enough history to recover from loss or request a new baseline.

Snapshots are immutable after publication. The Game Thread serializes prepared data
during Game Thread egress; serialization never reads a mutating simulation.

Replication capture writes schema-selected properties into paged linear storage
and records entity/property offsets. Per-connection builders use retained
baseline history and packet arenas. There is no heap allocation per replicated
property or virtual call per connection/property pair. A schema can provide a
class-level packed capture function while retaining property metadata for tools
and compatibility.

Dirty bits are a capture optimization, not the source of truth. Mutating a
replicated field goes through its typed setter or class capture policy, which
marks the relevant group. Debug/test configurations periodically perform a full
capture comparison to detect missed dirtiness. Creates and fresh baselines
always capture complete initial state regardless of dirty flags.

Only an explicitly acknowledged snapshot can become a delta baseline. If the
required baseline has expired, the server sends a fresh baseline rather than
constructing a delta against guessed client state. History and per-connection
work are bounded by tick window, bytes, and entity count.

### 10. Relevance and interest management

Not every client receives every entity. Relevance can use:

- scene or spatial region;
- distance and visibility candidates;
- ownership;
- team or audience;
- always-relevant game state;
- explicit entity policy;
- dependency relevance, such as a possessed pawn's weapon.

Relevance affects transmission, not server existence. Losing relevance removes
or dormants the client's representation according to class policy; it never
destroys the authoritative server entity.

Interest management begins with a simple spatial index and explicit overrides.
More complex prioritization is added only after bandwidth and CPU measurement.

### 11. Client application of snapshots

The client applies received data at a defined simulation safe point:

1. validate packet and snapshot sequence;
2. reject deltas whose baseline is unavailable;
3. stage required creates and a temporary combined old-plus-staged ID map;
4. decode and validate all property changes and references into staging storage;
5. commit destroys, creates, and property changes in protocol-defined order;
6. publish the new ID map and append interpolation samples;
7. reconcile predicted state if acknowledged commands are present;
8. emit presentation events only after the complete snapshot is visible.

Malformed snapshots fail without exposing partially created entities. A client
requests a new baseline or disconnects according to error severity.

### 12. Prediction and reconciliation

Only the locally controlled entity and explicitly declared dependent objects are
normally predicted.

```text
client creates command N
  -> record pre-command predicted state
  -> simulate command N immediately
  -> send command N

server receives command N
  -> simulate authoritatively
  -> acknowledge N with authoritative state

client receives acknowledgement
  -> compare against saved prediction
  -> restore authoritative state if different
  -> replay commands N+1 through latest
  -> smooth presentation-only correction
```

Shared predicted simulation must be deterministic enough for its declared inputs
and fixed tick. Backend physics results that cannot be reproduced reliably are
treated as server corrections rather than assumed deterministic.

Each predicted feature declares one packed `PredictionState`: all mutable values
needed to restore and replay it, including dependent entity state and random
stream position when applicable. Replay suppresses ordinary presentation side
effects and emits only tokenized effects whose deduplication policy permits it.
Prediction code cannot read presentation time, OS input, untracked mutable
globals, or non-replayable client physics state.

Prediction history has a bounded memory and tick window. Missing history causes
a hard correction rather than unsafe replay.

### 13. Interpolation

Remote entities render at a presentation time behind the newest received server
state. Each interpolated property stores a short timestamped history.

The client interpolates transforms and other declared values between valid
samples. Short missing intervals can use bounded extrapolation only when the
class policy permits it. Teleports and discontinuities explicitly reset history.

Interpolation changes presentation state, not authoritative or predicted
simulation state.

### 14. Game events

Reliable persistent outcomes should normally be represented by replicated state.
Transient events can be sent explicitly when presentation requires them:

- impact or muzzle-flash events;
- announcer messages;
- one-shot effects;
- confirmed hit feedback.

Events declare reliability, audience, ordering, and late-arrival policy. A
transient event must not be the only representation of durable game state.

### 15. Scene coordination

The server negotiates one scene identity and content revision before activation.
Both sides load the complete cooked scene locally; static scene data is not sent
over the game connection. The session uses that scene until disconnect.

All activation-critical assets for network classes that may spawn during the
session are declared by the scene/game manifest and loaded before play. Runtime
spawn cannot trigger synchronous storage I/O during a tick.

### 16. Threading

- The Game Thread polls nonblocking sockets at ingress and egress.
- Packet parsing validates untrusted byte ranges before producing messages.
- Server replication reads an immutable post-tick snapshot.
- Client snapshot application occurs on the client simulation thread.
- Prediction executes on the client simulation thread.
- There are no network callbacks or network-owned threads.

### 17. Security and failure behavior

- Treat every network byte, command, and client request as untrusted.
- Bounds-check lengths, indices, counts, decompression sizes, and identifiers.
- Rate-limit expensive requests and malformed-packet diagnostics.
- Authenticate connections before granting a player identity.
- Never expose server pointers, local handles, filesystem paths, or backend IDs.
- Disconnect on protocol violations that make continued interpretation unsafe.
- Prefer correction and diagnostics for ordinary packet loss or stale data.
- Bound per-connection entities, creates, references, reliable bytes, fragments,
  commands, discrete requests/results, snapshot history, and decode work before
  allocating or iterating.
- Reliable-queue exhaustion disconnects or rejects the producer according to
  protocol policy; it never turns into unbounded memory growth.

### 18. Invariants

- The server is the final authority.
- Transport code does not contain game rules.
- A `NetEntityId` is never used as a local memory or slot index without mapping.
- Replication reads stable post-tick data, not a mutating simulation.
- Client prediction can never commit an authoritative outcome.
- Static shared content is referenced by validated asset revision.
- Durable state is not represented solely by transient events.
- Snapshot deltas use acknowledged baselines only.
- All prediction-mutated state is captured and restorable.
- Sequence and tick wrap use protocol modular comparison, never raw ordering.
- Discrete client requests are validated, bounded, and applied at most once.
