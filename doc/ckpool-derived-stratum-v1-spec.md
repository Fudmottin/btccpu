# ckpool-derived Stratum V1 Specification


## Abstract

This document specifies a ckpool-compatible profile of the Stratum V1 mining protocol.

Stratum V1 does not have a single canonical, formally standardized specification. Its behavior has historically emerged from implementation practice and informal documentation.

This document defines a precise, byte-level and message-level specification sufficient to implement interoperable miners and pools compatible with ckpool behavior.

## 1. Scope

### 1.1 Scope clarification

This document specifies a ckpool-derived subset of Stratum V1 sufficient for practical interoperability.

The following methods are defined:

- `mining.subscribe`
- `mining.authorize`
- `mining.set_difficulty`
- `mining.notify`
- `mining.submit`

Other Stratum V1 methods and extensions are out of scope unless explicitly specified.

This document does not claim to define all historical or variant Stratum V1 behaviors.


This document specifies the Stratum V1 mining protocol sufficient to implement interoperable clients and servers.

This specification is:

- **Normative** where explicitly stated
- **Derived from ckpool behavior** where the protocol is under-specified
- **Annotated** where behavior reflects ecosystem practice rather than strict guarantees

This document focuses on:

- Message formats
- Field semantics
- Byte-level reconstruction rules
- Share validation behavior

---

## 2. Terminology

### 2.1 Byte sequence
An ordered sequence of 8-bit values.

### 2.2 Hex string
ASCII encoding of a byte sequence:
- Two hex characters per byte
- Big-endian textual representation

### 2.3 Display order
The order in which bytes are represented in hex strings.

### 2.4 Internal order
The byte order used during hashing and internal computation.

### 2.5 Word
A 32-bit (4-byte) unit.

### 2.6 dSHA256
Double SHA-256:
dSHA256(x) = SHA256(SHA256(x))

---

## 3. Transport and Framing

### 3.1 Transport
- TCP stream

### 3.2 Encoding
- UTF-8 JSON

### 3.3 Framing
- Messages are newline-delimited
- No length prefix

---

## 4. JSON Encoding Rules

### 4.1 Binary data
All binary values SHALL be encoded as hex strings.

### 4.2 Numbers
- Integers: JSON numbers
- Difficulty: JSON number (may be non-integer in ecosystem practice)

---

## 5. Session Lifecycle

1. Client connects via TCP
2. Client sends `mining.subscribe`
3. Server responds with:
   - extranonce1
   - extranonce2_size
4. Client sends `mining.authorize`
5. Server sends:
   - `mining.set_difficulty`
   - `mining.notify`
6. Client begins mining and submitting shares

---

## 6. Method Specifications

### 6.1 mining.subscribe

#### Request

The client SHALL send a JSON-RPC request whose method is `mining.subscribe`.

A minimal request has the form:

```json
{"id":1,"method":"mining.subscribe","params":[]}
```

This specification does not require any particular parameter list for the request. A server MAY accept zero or more implementation-specific parameters consistent with common Stratum V1 practice.

#### Response

The response `result` SHALL be a JSON array of the form:

```text
[subscriptions, extranonce1, extranonce2_size]
```

```json
{"id":1,"result":[[["mining.notify","subscription-id"]],"abcdef01",4],"error":null}
```

Where:

* `subscriptions` is an implementation-defined subscription descriptor
* `extranonce1` is a hex string representing an opaque byte sequence assigned by the server
* `extranonce2_size` is an integer giving the required size, in bytes, of miner-supplied `extranonce2`

For ckpool-compatible operation, the client SHALL persist `extranonce1` and `extranonce2_size` for subsequent coinbase reconstruction and share submission.

---

### 6.2 mining.authorize

#### Request

The client SHALL send a JSON-RPC request whose method is `mining.authorize` and whose parameters are:

```text
[workername, password]
```

```
{"id":2,"method":"mining.authorize","params":["workername","password"]}
```

Where:

* `workername` is a JSON string
* `password` is a JSON string

#### Response

The response `result` SHALL be a JSON boolean.

If `true`, the client is authorized to submit shares for that session.

If `false`, the client SHALL treat authorization as failed.

---

### 6.3 mining.set_difficulty

#### Parameters

The server SHALL send:

```text
[difficulty]
```

```json
{"id":null,"method":"mining.set_difficulty","params":[1]}
```

Where:

* `difficulty` is a JSON number

For ckpool-compatible operation, this value defines the active share difficulty threshold against which subsequent submitted shares are evaluated, subject to server job and state handling.

---

### 6.4 mining.notify

#### Parameters

The server SHALL send `mining.notify` with parameters of the form:

```text
[
  job_id,
  prevhash,
  coinbase1,
  coinbase2,
  merkle_branch,
  version,
  nbits,
  ntime,
  clean_jobs
]
```

```json
{"id":null,"method":"mining.notify","params":["job-id","prevhashhex","coinbase1hex","coinbase2hex",["merkle1hex","merkle2hex"],"20000000","1d00ffff","65f2c2b0",true]}
```

#### Field definitions

| Field         | Type                  | Description                                      |
| ------------- | --------------------- | ------------------------------------------------ |
| job_id        | string                | job identifier                                   |
| prevhash      | hex string (32 bytes) | previous block hash in protocol representation   |
| coinbase1     | hex string            | coinbase prefix                                  |
| coinbase2     | hex string            | coinbase suffix                                  |
| merkle_branch | array of hex strings  | ordered merkle path elements                     |
| version       | hex string (4 bytes)  | block version                                    |
| nbits         | hex string (4 bytes)  | compact target                                   |
| ntime         | hex string (4 bytes)  | timestamp                                        |
| clean_jobs    | boolean               | indicates whether prior jobs should be discarded |

For ckpool-compatible operation:

* `merkle_branch` elements SHALL each decode to 32 bytes
* `version`, `nbits`, and `ntime` SHALL each decode to 4 bytes
* `prevhash` SHALL decode to 32 bytes

A client SHALL associate the received notify fields with the received `job_id`.

---

### 6.5 mining.submit

#### Parameters

The client SHALL send `mining.submit` with parameters of the form:

```text
[workername, job_id, extranonce2, ntime, nonce]
```

or, where version rolling is supported by both sides:

```text
[workername, job_id, extranonce2, ntime, nonce, version_mask]
```

Where:

* `workername` is a JSON string
* `job_id` is a JSON string
* `extranonce2` is a hex string
* `ntime` is an 8-hex-character string representing 4 bytes
* `nonce` is an 8-hex-character string representing 4 bytes
* `version_mask`, if present, is a hex string representing a 32-bit value

```json
{"id":3,"method":"mining.submit","params":["workername","job-id","00000000","65f2c2b0","00000001"]}
```

For ckpool-compatible operation:

* `extranonce2` SHALL decode to exactly `extranonce2_size` bytes

* `ntime` SHALL be valid hex and parse as a 32-bit unsigned integer
* `nonce` SHALL be valid hex and decode to 4 bytes

---
## 7. Field Semantics

### 7.1 prevhash

The `prevhash` field is a protocol-defined value.

In ckpool-compatible generation, it is derived from the Bitcoin RPC `previousblockhash` by:

1. hex-decoding the 32-byte RPC hash
2. applying `swap_256`
3. hex-encoding the transformed 32-byte result for transmission

Therefore, the `prevhash` value carried in `mining.notify` is not a direct copy of the Bitcoin RPC display string.

A client SHALL:

- treat `prevhash` as a protocol field with its own reconstruction semantics
- decode it from hex only when reconstructing the candidate header through the Section 8 pipeline

A client SHALL NOT assume that `prevhash` is in Bitcoin RPC display order or directly usable as final hash input.

---

### 7.2 extranonce

- `extranonce1` is assigned by the server and SHALL be treated as an opaque byte sequence
- `extranonce2` is provided by the client
- `extranonce2` SHALL decode to exactly `extranonce2_size` bytes

---
## 8. Byte-Order and Reconstruction Rules

### 8.0 Representation domains

The protocol operates across multiple distinct representations:

1. JSON hex string representation (wire format)
2. Decoded byte sequence (post-hex decoding)
3. Intermediate header representation (pre-final transform)
4. Hash input representation (post-final transform)

These representations SHALL NOT be treated as interchangeable.

This section is normative.

All transformations described in this section SHALL be applied exactly as specified.

An implementation SHALL NOT substitute alternative byte-order assumptions.

### 8.1 Coinbase reconstruction

```
coinbase =
    decode_hex(coinbase1)
 || decode_hex(extranonce1)
 || decode_hex(extranonce2)
 || decode_hex(coinbase2)
```

---

### 8.2 Merkle root reconstruction

Let CB be the reconstructed coinbase byte sequence.

The initial value SHALL be:

R0 = dSHA256(CB)

For each branch Bi in merkle_branch[], in order:

Ri+1 = dSHA256(Ri || decode_hex(Bi))

Where:
- decode_hex(Bi) yields exactly 32 bytes
- concatenation is byte concatenation

No byte reversal SHALL be applied to Ri or to any branch value.

---

### 8.3 Transform definitions

For ckpool compatibility, the following transforms are normative.

#### flip_32

`flip_32` operates on a 32-byte value interpreted as eight consecutive 32-bit words.

For each word independently:

- the 4 bytes of the word are reversed
- the order of the eight 32-bit words is preserved

Equivalently, if the input words are:

`W0 W1 W2 W3 W4 W5 W6 W7`

then the output is:

`bswap32(W0) bswap32(W1) bswap32(W2) bswap32(W3) bswap32(W4) bswap32(W5) bswap32(W6) bswap32(W7)`

#### flip_80

`flip_80` operates on an 80-byte value interpreted as twenty consecutive 32-bit words.

For each word independently:

- the 4 bytes of the word are reversed
- the order of the twenty 32-bit words is preserved

Equivalently, if the input words are:

`W0 W1 ... W19`

then the output is:

`bswap32(W0) bswap32(W1) ... bswap32(W19)`

#### swap_256

`swap_256` operates on a 32-byte value interpreted as eight consecutive 32-bit words.

- the order of the eight 32-bit words is reversed
- the bytes within each 32-bit word are not changed

Equivalently, if the input words are:

`W0 W1 W2 W3 W4 W5 W6 W7`

then the output is:

`W7 W6 W5 W4 W3 W2 W1 W0`

#### bswap_256

`bswap_256` operates on a 32-byte value interpreted as eight consecutive 32-bit words.

- the order of the eight 32-bit words is reversed
- the 4 bytes within each 32-bit word are also reversed

Equivalently, if the input words are:

`W0 W1 W2 W3 W4 W5 W6 W7`

then the output is:

`bswap32(W7) bswap32(W6) bswap32(W5) bswap32(W4) bswap32(W3) bswap32(W2) bswap32(W1) bswap32(W0)`

---

### 8.4 Intermediate merkle transform

Let `R` be the 32-byte merkle root produced by the folding procedure in Section 8.2.

The value inserted into the intermediate header buffer SHALL be:

`flip_32(R)`

No other transform SHALL be applied to the merkle root before that insertion step.

---

### 8.5 Header reconstruction

The logical Bitcoin header fields are:

- version (4 bytes)
- prevhash (32 bytes)
- merkle_root (32 bytes)
- ntime (4 bytes)
- nbits (4 bytes)
- nonce (4 bytes)

#### Stage 1: Intermediate header buffer

An 80-byte intermediate header buffer SHALL be constructed as follows:

- copy the cached job-specific header template
- insert `flip_32(merkle_root)` at byte offset 36
- insert submitted `ntime32` at byte offset 68 as `htobe32(ntime32)`
- decode the submitted nonce from 8 hex characters to 4 bytes and insert those 4 bytes at byte offset 76

If `version_mask` is present:

- convert `version_mask` with `htobe32`
- bitwise-OR the resulting 32-bit value into the version field at offset 0

#### Stage 2: Hash input header

Apply `flip_80` to the entire 80-byte intermediate header buffer.

The resulting 80-byte sequence SHALL be the exact input to the first SHA-256 invocation.

#### Stage 3: Proof-of-work hash

The proof-of-work hash SHALL be:

`dSHA256(flip_80(intermediate_header))`

---

## 9. Share Submission Validation

### 9.1 Binding to job state

A submitted share SHALL be evaluated against:

- the job identified by `job_id`
- the session state associated with the submitting client

That evaluation SHALL include:

- notify-derived job data
- `extranonce1` assigned to the session
- submitted `extranonce2`
- submitted `ntime`
- submitted `nonce`
- submitted `version_mask`, if present and supported

---

### 9.2 Field validity

The following conditions SHALL be satisfied:

- `extranonce2` SHALL decode to exactly `extranonce2_size` bytes
- `ntime` SHALL be valid hex and parse as a 32-bit unsigned integer
- `nonce` SHALL be valid hex and decode to exactly 4 bytes

A share SHALL be rejected if any of these conditions fail.

---

### 9.3 ntime validity

Let:

- `job_ntime` = uint32 parsed from notify `ntime`
- `submitted_ntime` = uint32 parsed from submit `ntime`

A share SHALL be rejected unless:

job_ntime <= submitted_ntime <= job_ntime + 7000

The comparison SHALL be performed on 32-bit unsigned integers.

---

### 9.4 Share hash validation

The server SHALL:

1. reconstruct the candidate header according to Section 8
2. compute:

   share_hash = dSHA256(candidate_header)

A share SHALL be accepted only if `share_hash` satisfies the server's active share target.

A share MAY also satisfy the full block target. Block-solution handling is distinct from ordinary share acceptance.

---

## 10. Error Handling

### 10.1 Invalid share

A submitted share MAY be rejected for reasons including:

- invalid JSON or parameter structure
- unknown or stale `job_id`
- invalid `extranonce2` length
- invalid `ntime`
- invalid `nonce`
- insufficient share difficulty
- stale job state

This specification does not define a universal Stratum V1 error taxonomy.

---

## 11. Acceptance Behavior

For each submitted share, the server SHALL:

1. resolve the referenced job and session state
2. reconstruct the coinbase transaction
3. reconstruct the merkle root
4. construct the candidate header according to Section 8
5. compute the proof-of-work hash:

   share_hash = dSHA256(candidate_header)

6. compare `share_hash` against the active share target
7. determine separately whether the candidate also satisfies the full block target
8. accept or reject the share accordingly

---

## 12. Conformance


A compliant implementation SHALL:

- Follow all reconstruction rules exactly
- Respect byte-order transformations
- Enforce validation constraints

---
## 13. References

- Bitcoin Wiki — Stratum mining protocol  
  https://en.bitcoin.it/wiki/Stratum_mining_protocol

- BIP 310 — Stratum protocol extensions  
  https://en.bitcoin.it/wiki/BIP_0310

- ckpool source code  
  https://bitbucket.org/ckolivas/ckpool/src/master/


---

## 14. ckpool Compatibility Notes

The following behaviors are derived from ckpool:

- Merkle branches are not reversed
- Coinbase hash is used directly as merkle leaf
- Header is transformed via intermediate + flip_80 pipeline
- prevhash is transformed from RPC representation

---

## 15. Open Ambiguities

The following remain implementation-defined in the wider Stratum V1 ecosystem:

- difficulty rounding and comparison behavior across different pool implementations
- exact treatment of optional version rolling parameters outside ckpool-compatible behavior
- behavior of rarely used extension methods not covered in this document

---

## 16. Non-Goals

This specification does not define:

- Stratum V2
- DATUM protocol
- Pool payout logic
- Bitcoin consensus rules

---

<p align="center"><em>This document was created with an LLM. You can tell, can't you?</em></p>

