# Admission example: one policy, two formats

This example runs one node's two inbound paths through the admission framework in
`include/admissionspec/`:

- **Protobuf peer frames** from a small, made-up gossip protocol (`gossip.proto`).
- **JSON RPC requests** to the same node's endpoint.

Both paths use the same spec type, the same three stages, the same refusal rules and penalty
classes, and the same sinks. They differ only in how a message is named, which walker reads the
body, and what "privileged" means.

Build and run it, from a test build with protobuf (see the top-level README):

```sh
conan install . -o tests=True -o with_protobuf=True -s build_type=Release --build=missing -g CMakeDeps
cmake --preset conan-release
cmake --build build/Release --target rpcspec_admission_example
build/Release/examples/admission/rpcspec_admission_example
```

## The files

| File | What it holds |
|---|---|
| `gossip.proto` | The peer protocol's messages: `Ping`, `Inventory`, `AdminReport`, `Fetch`. |
| `GossipSpecs.hpp` | One spec per message, attached to the generated classes through `AdmissionTraits`, and the `GossipSpecs` registry. |
| `RpcSpecs.hpp` | One spec per RPC method, attached to the example's own request types through `admissionSpec` overloads (the route for types you own), and the `RpcSpecs` registry. |
| `ReputationSink.hpp` | The node's lasting record of a sender, a `ChargeSink` whose fees are fixed in code. |
| `Receipt.hpp` | What became of one message. |
| `GossipHost.hpp` | The peer receive path: framing, the stages, closing a connection. |
| `RpcHost.hpp` | The RPC request path: the method from the path, the stages, error responses. |
| `main.cpp` | Feeds both paths a script of messages and prints a receipt for each. |

## 1. Declaring a policy: specs

A spec is the whole admission policy of one message type. It has three parts.

- **Tunables.** These are named values that configuration may override.
  - `max_payload_bytes` and `size_ramp` are required in every spec. The first caps both declared
    sizes. The second prices a message by its decoded size.
  - A spec may declare more, as `Fetch` does with `max_limit`.
- **Attributes.** These are facts about the protocol, not settings:
  - the type id the frame names;
  - whether only a privileged sender may send it;
  - a feature the connection must have negotiated;
  - whether older releases pass it on as received;
  - caps on repeated fields.
- **A check**, optionally. It sees every value as the payload is walked, for rules a cap cannot
  state.

The protobuf side, from `GossipSpecs.hpp`:

```cpp
template <>
struct AdmissionTraits<gossip::Inventory>
{
    static consteval auto spec()
    {
        return makeSpec<gossip::Inventory>(
                   tunable<"max_payload_bytes">(uint64_t{16 * 1024}, "admission.inventory.max_payload_bytes"),
                   tunable<"size_ramp">(ramp({{.upToBytes = 1024, .cost = 1.0},
                                              {.upToBytes = 16 * 1024, .cost = 4.0}}),
                                        "admission.inventory.size_ramp"))
            .withAttributes({.typeId = 2,
                             .relayedByOlderVersions = true,
                             .repeated = repeatedCaps({{.fieldNumber = gossip::Inventory::kHashesFieldNumber,
                                                        .maxEntries = 256,
                                                        .minEntryBytes = 32}})});
    }
};
```

The JSON side, from `RpcSpecs.hpp`. It is the same shape, but the cap names its array by key
instead of by field number:

```cpp
consteval auto admissionSpec(std::type_identity<SubmitRequest>)
{
    return makeSpec<SubmitRequest>(/* max_payload_bytes, size_ramp */)
        .withAttributes({.typeId = static_cast<uint32_t>(Method::Submit),
                         .repeated = repeatedCaps({{.key = "tx_blobs", .maxEntries = 8, .minEntryBytes = 64}})});
}
```

A spec is a `consteval` value, so a malformed one does not compile. Examples:

- a missing `max_payload_bytes`;
- a cap that names both a field number and a key;
- two specs in one registry with the same type id.

A registry (`GossipSpecs`, `RpcSpecs`) maps the type id that arrives at run time to the C++ type
whose spec governs it.

## 2. What the host supplies

The framework holds no state and closes nothing. The host supplies four things.

1. **A `ConnectionView`.** This is what the stages may ask about the sender:
   - `privileged()`: trusted for a peer, the admin role for a client;
   - `hasFeature(id)`;
   - `compressionNegotiated()`.
2. **One or more `ChargeSink`s.** This is where each decision's cost is charged.
   - `ReputationSink` is the lasting record. It charges the larger of the stage's cost and the
     fee for the penalty class, so a refusal never costs less than admitting a message of the same
     size.
   - The fee table (Trivial 1, Useless 50, Invalid 200, Malformed 1000) is fixed in the host's
     code. Those fees decide when an honest sender is cut off, so they must not vary from node to
     node.
   - A `TokenBucketSink` limits how fast one connection may send.
   - `AllSinks{reputation, bucket}` charges both and reports the worse standing.
3. **Framing.** This means finding the type and the declared size before reading the body: a
   frame header for a peer, the request path and Content-Length for RPC.
4. **What to do at `Standing::Drop`.**

## 3. The three stages

Each message passes through up to three stages. Both hosts follow the same order.

### Stage 1: the header (`admitHeader<T>`)

This stage decides from `FrameInfo`, which holds the type, the wire and decoded sizes, and whether
the payload is compressed. Nothing of the payload has been read at this point. In order, it checks:

1. **Is the sender already at Drop?** If so, the message is refused unread and not charged again
   (`Rule::Sender`).
2. **The connection gate.**
   - A privileged-only type from an unprivileged sender is refused (`Rule::Privileged`).
   - So is a type whose feature the connection did not negotiate (`Rule::Feature`).
3. **Compression.** A compressed payload on a connection that never negotiated compression is
   refused (`Rule::Compression`).
4. **Size.** Either declared size over `max_payload_bytes` is refused (`Rule::Oversized`).

An admission is charged the size ramp. A refusal carries both its penalty class and the ramp's
cost, and this host's sinks charge the larger of the two. A type older releases relay is the
exception: its refusals carry `Trivial` and no cost.

A type the registry does not know never reaches a spec. The host refuses it itself, with
`Rule::UnknownType`.

**An admitted message can still leave the sender at Drop.** In the run below, Peer C's 6th ping
and Client F's 2nd request are admitted, but their charge takes the token bucket past what it
holds. Each host checks the returned `standing` and refuses them (`Rule::Sender`). A host that only
looked at the decision would serve them anyway.

### Stage 2: the body walk (`admitBody<T>(walker, ...)`)

The walker reads the payload before anything is built from it, and feeds each value to the spec's
repeated-field caps and then to its check. The first refusal stops the walk.

| | Protobuf | JSON |
|---|---|---|
| Walker | `protobufVisitor<T>(stream, size)` or `protobufVisitor<T>(span)` | `jsonVisitor(span, options)` |
| One call, outside the pipeline | `prescanProtobuf<T>(span)` | `prescanJson<T>(span)` |
| A cap names | a field number, counted at the top level | a key, counting that array's elements |
| Values reach a check as | the field's declared type: `Int64`, `Uint64`, `String`, `Bytes`, ... | `Int64`, `Uint64`, `Double`, `String`, ... with their `key` |
| Refuses on its own | malformed wire data, more unknown fields in one message than allowed, groups, nesting too deep | invalid JSON, nesting deeper than `maxDepth` |

The protobuf walker also takes a `ZeroCopyInputStream`, so a frame that arrived in several receive
buffers is walked where it lies. The gossip host feeds every payload in 16-byte pieces to show
this. A string or bytes value that spans two pieces reaches the check with its `size` but no
`value`. The `Fetch` check needs only the hash's size, so it still works when the 32-byte hash is
split.

The protobuf walker also counts the unknown fields in the whole payload, and their bytes, and
`admitBody` returns them in `StageResult::totals`, whether the message is admitted or refused. A
host can record them before it chooses limits, to see what its honest traffic carries. Walkers
that do not count, such as the JSON walker, report zeros.

Each refusal carries a `Rule` that names the rule it broke: `RepeatedCap`, `EntrySize`,
`UnknownFields`, `Group`, `Depth`, `Malformed`, or `Check` for the spec's own check. A host that
charges or reports rules differently tells them apart by this, never by the reason text.

### Stage 3: parse and handle

Only now is the message built: `ParseFromArray` for protobuf, `boost::json::parse` for JSON. The
protobuf walker is held to accepting exactly what libprotobuf's parser accepts by a differential
test, so a payload cannot mean one thing to admission and another to the parse. The handler then
enforces whatever needs state, such as whether a reply answers a request this node made.

## 4. The run, line by line

`balance` is the sender's `ReputationSink` balance and `tokens` what is left in its bucket, both
after the message.

### Peer A: untrusted, without the fetch feature

```
ping                                   handled  body   -             -               1   99.0
inventory of 3 hashes                  handled  body   -             -               2   98.0
inventory with a 4-byte hash           refused  body   EntrySize     Trivial         4   96.0
inventory of 300 hashes (cap 256)      refused  body   RepeatedCap   Trivial         9   91.0
inventory of 600 hashes (over 16 KiB)  refused  header Oversized     Trivial        10   90.0
admin report                           refused  header Privileged    Useless        60   85.0
fetch                                  refused  header Feature       Useless       110   80.0
ping with 17 unknown fields            refused  body   UnknownFields Invalid       311   69.0
type 9, which has no spec              refused  header UnknownType   Useless       361   64.0
ping that does not decode              CLOSED   body   Malformed     Malformed    1362   43.0
ping after the close                   CLOSED   closed -             -            1362   43.0
```

- **The three inventory refusals cost 1 each, plus the ramp.** `Inventory` is
  `relayedByOlderVersions`, so the sender may be an honest relay passing on what it received.
- **The 600-hash inventory is refused at stage 1.** Its body is never read.
- **The admin report and the fetch are refused at stage 1 too.** They fail the connection gate, so
  their bodies are never read.
- **Unknown fields are counted per message.** 17 in one message is over the default limit of 16.
- **The undecodable ping closes the connection.** A payload that cannot be read is charged
  `Malformed`, and the gossip host closes. Here the fee alone also takes the balance past Drop.

### Peer B: trusted, with the fetch feature

```
admin report                           handled  body   -             -               1   99.0
fetch, limit 10                        handled  body   -             -               3   97.0
fetch, limit 500 (max 100)             refused  body   Check         Invalid       205   85.0
fetch of a 20-byte hash                refused  body   Check         Invalid       407   73.0
```

The same two types that were refused for Peer A are now admitted. The `Fetch` check then refuses a
limit over its tunable and a hash that is not 32 bytes. No cap could express either rule.

### Peer C: a burst of 5 tokens, refilling at 1 a second

```
ping 5                                 handled  body   -             -               5    0.0
ping 6                                 refused  header Sender        -               6    0.0
```

The bucket is empty, so the sender is refused. Its balance is far from Drop, so the connection
stays open. The bucket controls flow; only the lasting record cuts a sender off.

### Clients D, E and F: the same, over JSON

```
/v1/server_info {}                     handled  body   -             -               1   99.0
/v1/submit (2 blobs)                   handled  body   -             -               3   97.0
/v1/submit (9 blobs, cap 8)            refused  body   RepeatedCap   Invalid       205   85.0
/v1/submit (a 10-character blob)       refused  body   EntrySize     Invalid       407   73.0
/v1/submit (over 64 KiB)               refused  header Oversized     Useless       457   63.0
/v1/history limit 50                   handled  body   -             -             460   60.0
/v1/history limit 5000 (max 200)       refused  body   Check         Invalid       663   47.0
/v1/history nested 9 deep              refused  body   Malformed     -             676   34.0
/v1/history cut off                    refused  body   Malformed     -             689   21.0
/v1/stop {}                            refused  header Privileged    Useless       739   16.0
/v1/teleport {}                        refused  header UnknownType   Useless       789   11.0
```

- **Every rule from the peer side has a JSON counterpart.**
  - The array cap and the minimum entry size apply to the `tx_blobs` key.
  - Size is checked from Content-Length, before the body is read.
  - The `History` check bounds `limit`.
  - The admin role gates `/v1/stop` (Client E, an admin, is served).
  - An unknown method is the host's `UnknownType`.
- **Invalid JSON is refused, but not closed.** The JSON walker charges a fixed cost for it
  (`VisitJsonOptions::costForInvalidPayload`) rather than a penalty class. An RPC host answers it
  with an error, so nothing is closed.
- **Client F is refused on its second request.** A history request costs 3 tokens, and after the
  first only 2 are left.

## Where to go from here

- **Configuration.** Each tunable has a config key. `resolve(spec, config)` applies a host's
  overrides; `admitHeader` and `admitBody` use the defaults.
- **The method inside the body.** A JSON-RPC request names its method in the body, not the path.
  Finding it needs a small first pass over the body that stops at the method key. Stage 1 then
  runs as here.
