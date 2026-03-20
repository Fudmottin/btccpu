import hashlib

def dblsha(b: bytes) -> bytes:
    return hashlib.sha256(hashlib.sha256(b).digest()).digest()

def flip32_words(b: bytes) -> bytes:
    if len(b) % 4 != 0:
        raise ValueError("length must be a multiple of 4")
    out = bytearray()
    for i in range(0, len(b), 4):
        w = b[i:i+4]
        out.extend(w[::-1])
    return bytes(out)

job_id = "69b23e1000005a88"
prevhash = "60a003fb36548de6ed395d227308a62488b0d1c5000169d70000000000000000"
coinb1 = (
    "0100000001000000000000000000000000000000000000000000000000000000"
    "0000000000ffffffff3503335d0e0004609ebc69047e483d0d10"
)
extranonce1 = "3e3eb26900000000"
extranonce2 = "0000000000000000"
coinb2 = (
    "0a636b706f6f6c0d2f42697441786520427272722fffffffff02f0c8a5120000"
    "0000160014f42e1a5f41c23247de0022aca1b069ca3e43e0bc00000000000000"
    "00266a24aa21a9edae14689538cbacfe37f6e55e5acb11f6528600ede90d01f52"
    "328a7154468280700000000"
)

branches = [
    "763fc3374b1d35c9110f4424d6a86bfc5057eab9e20419f40667b25105d37f9b",
    "19f4561c51dab0f5af712b1d833405cbb75bad91114abc575b92aa0d6eb324b0",
    "12ac175d0ae5a54a79ff439a0ad8bb1e3d9495c7226587da73c2af009db13b39",
    "836776ec508139f7dc8058c5bcc320eddaedc85bac297f3c469a080cfc4d4ed4",
    "8a58e8348bfbce05b7cd5834877be291511b9293008ad49e8ccd125cf29a6613",
    "92a866abe78e22a31c3eefa7c36e2049e6bac9fe8242eaa065414b67688a7b2b",
    "df3aa4a153486c615b79263388f1fe1d8adc48bdc0467310795da2d7d57866ad",
    "04d8a5f87d67d225ce47dd8049dba0e21eb3d6dcd34e89a90d688d721812fdff",
    "ea1b30222b36b1b7657a7777a7da0c347d2307235255379f27c2ee856dc96501",
]

version = "20000000"
ntime = "69bc9e60"
nbits = "1701f0cc"
nonce = "0525050b"

coinbase_hex = coinb1 + extranonce1 + extranonce2 + coinb2
coinbase = bytes.fromhex(coinbase_hex)
coinbase_hash = dblsha(coinbase)

print("coinbase hex:", coinbase_hex)
print("coinbase hash raw:    ", coinbase_hash.hex())
print("coinbase hash display:", coinbase_hash[::-1].hex())

merkle = coinbase_hash
for branch_hex in branches:
    branch = bytes.fromhex(branch_hex)
    merkle = dblsha(merkle + branch)

print("merkle root raw:      ", merkle.hex())
print("merkle root display:  ", merkle[::-1].hex())

merkle_in_header = flip32_words(merkle)
prevhash_in_header = flip32_words(bytes.fromhex(prevhash))

print("prevhash in header:   ", prevhash_in_header.hex())
print("merkle in header:     ", merkle_in_header.hex())

headerbin = bytearray.fromhex(
    version
    + prevhash
    + ("00" * 32)
    + ntime
    + nbits
    + nonce
)

headerbin[36:68] = merkle_in_header

print("headerbin:            ", bytes(headerbin).hex())

header_for_hash = flip32_words(bytes(headerbin))
print("header for hash:      ", header_for_hash.hex())

header_hash = dblsha(header_for_hash)

print("header hash raw:      ", header_hash.hex())
print("header hash display:  ", header_hash[::-1].hex())

