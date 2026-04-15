<div align="center">

# Binary Signing Tool

**Host-side binary signing tool for embedded systems — secure bootloader and OTA update workflows**

![Language](https://img.shields.io/badge/language-C99-blue.svg)
![Crypto](https://img.shields.io/badge/crypto-EdDSA%20%2F%20BLAKE2b-green.svg)
![Library](https://img.shields.io/badge/library-Monocypher%204.0.1-lightgrey.svg)
![Build](https://img.shields.io/badge/build-CMake-red.svg)
![License](https://img.shields.io/badge/license-BSD--2--Clause%20%2F%20CC0-orange.svg)
![Platform](https://img.shields.io/badge/platform-Linux-lightblue.svg)

</div>

---

## Overview

This tool is a **host-side binary signing utility** designed to sign `.bin` files targeting deployment on embedded systems. A signed binary is the expected input for two common embedded security mechanisms:

- **Secure bootloader** — the bootloader runs on the target device and verifies the firmware signature against a stored public key before allowing execution. Only a binary signed with the matching private key will boot.
- **OTA (Over-The-Air) update** — before a firmware update image is accepted and flashed by the on-device OTA client, its signature is checked. This ensures that only authenticated firmware packages — signed on the host by a trusted build system — can be installed remotely.

This tool handles the **signing side** of that chain. It takes a raw `.bin` image, computes an EdDSA signature over its full content, and appends a compact **72-byte footer** containing the signature and metadata to produce a `.bin.sign` file. A verify mode is provided for integration testing and release validation on the host before the image is distributed.

The tool is fully self-contained: the cryptographic backend ([Monocypher](#cryptography)) is vendored as a single `.c` / `.h` pair with no external dependencies, keeping it portable across Linux build environments and straightforward to integrate into automated pipelines.

---

## Table of Contents

- [Project Structure](#project-structure)
- [Architecture](#architecture)
- [Signed File Format](#signed-file-format)
- [Dependencies](#dependencies)
- [Build](#build)
- [Documentation](#documentation)
- [Usage](#usage)
  - [sign](#sign----produce-a-signed-binary)
  - [verify](#verify----validate-a-signed-binary)
  - [Error conditions](#error-conditions)
- [Key Management](#key-management)
- [Cryptography](#cryptography)
- [Security Considerations](#security-considerations)
- [License](#license)

---

## Project Structure

```
signing_tool/
│
├── CMakeLists.txt                    # CMake build configuration
├── Doxyfile                          # Doxygen documentation config
│
├── keys/
│   └── keys.txt                      # Auto-generated Ed25519 key pair (hex-encoded, gitignored)
│
├── Core/
│   └── Src/
│       └── main.c                    # Entry point: CLI parsing, file I/O, footer assembly
│
├── Middleware/
│   ├── Inc/
│   │   └── crypto_dsa.h              # Public DSA API — types, constants, declarations
│   └── Src/
│       └── crypto_dsa.c              # Key lifecycle, hex encoding, sign/verify wrappers
│
└── Libraries/
    ├── Inc/
    │   └── monocypher.h              # Monocypher v4.0.1 — single-header interface
    └── Src/
        └── monocypher.c              # Monocypher v4.0.1 — full implementation (vendored)
```

---

## Architecture

The codebase is organized into three strictly layered modules, each with a single responsibility. Dependencies flow downward only — no layer calls into a layer above it.

```
╔══════════════════════════════════════════════════════╗
║                      main.c                          ║
║                                                      ║
║   CLI argument parsing  ·  file I/O                  ║
║   footer assembly  ·  sign / verify orchestration    ║
║                                                      ║
║   _sign_binary_file()                                ║
║   _verify_signature()                                ║
║   _generate_signed_binary_file()                     ║
╚═══════════════════════╦══════════════════════════════╝
                        ║  calls
╔═══════════════════════╩══════════════════════════════╗
║               crypto_dsa.c  /  crypto_dsa.h          ║
║                                                      ║
║   Key lifecycle  ·  hex encoding  ·  POSIX file I/O  ║
║                                                      ║
║   crypto_dsa_generate_keys()                         ║
║   crypto_dsa_sign()                                  ║
║   crypto_dsa_verify()                                ║
╚═══════════════════════╦══════════════════════════════╝
                        ║  calls
╔═══════════════════════╩══════════════════════════════╗
║            monocypher.c  /  monocypher.h             ║
║                                                      ║
║   EdDSA primitives over Curve25519 + BLAKE2b         ║
║                                                      ║
║   crypto_eddsa_key_pair()                            ║
║   crypto_eddsa_sign()                                ║
║   crypto_eddsa_check()                               ║
╚══════════════════════════════════════════════════════╝
```

`crypto_dsa` is the **sole entry point** into Monocypher. Application code in `main.c` never calls cryptographic primitives directly, keeping the crypto boundary explicit and auditable.

---

## Signed File Format

A signed file is a byte-for-byte copy of the original binary followed by a packed 72-byte footer:

```
 Offset 0                                        Offset N-1
 ┌──────────────────────────────────────────────────────────┐
 │                                                          │
 │               Original binary data                       │
 │                    (N bytes)                             │
 │                                                          │
 ├───────────────┬──────────────────┬───────────────────────┤
 │  magic        │  file_length     │  signature            │
 │  4 bytes      │  4 bytes         │  64 bytes             │
 │  0x424F4F54   │  N               │  R ‖ S  (EdDSA)       │
 │  "BOOT"       │  uint32_t LE     │  Curve25519 + BLAKE2b │
 └───────────────┴──────────────────┴───────────────────────┘
 ╰──────────────────────────────────────────────────────────╯
                         signed_file_footer_t
                            72 bytes total
                       __attribute__((packed))
```

| Field          | Type            | Size    | Description                                                    |
|----------------|-----------------|---------|----------------------------------------------------------------|
| `magic`        | `uint32_t`      | 4 bytes | `0x424F4F54` ("BOOT") — identifies a valid signed footer       |
| `file_length`  | `uint32_t`      | 4 bytes | Length of the original binary; used as message boundary on verify |
| `signature`    | `uint8_t[64]`   | 64 bytes | EdDSA signature `R ‖ S` over bytes `0..file_length-1`         |

**Key properties:**

- The footer struct is declared `__attribute__((packed))` — no compiler-inserted padding between fields.
- The signature covers **only** the original binary bytes (`0..N-1`), not the footer itself.
- On verification, the magic field is checked **before** any signature operation is attempted.
- Any modification to either the binary body or the `file_length` field will produce a signature mismatch and fail verification.

---

## Dependencies

| Dependency   | Version   | Purpose                                    | Notes                                 |
|--------------|-----------|--------------------------------------------|---------------------------------------|
| CMake        | ≥ 3.0     | Build system                               | —                                     |
| GCC / Clang  | C99       | Compiler                                   | Any C99-conforming toolchain works    |
| Linux        | any       | `getrandom(2)` syscall for entropy         | Requires `sys/random.h`               |
| Monocypher   | 4.0.1     | EdDSA / BLAKE2b cryptographic primitives   | Vendored — no external install needed |
| Doxygen      | ≥ 1.9     | API documentation generation               | Optional                              |

No external libraries need to be installed. Monocypher is bundled under `Libraries/` as a single `.c` / `.h` pair.

---

## Build

### Standard build

```bash
mkdir build && cd build
cmake ..
make
```

The compiled binary is placed at `build/signing_tool`.

### Verbose build

```bash
make VERBOSE=1
```

### Clean rebuild

```bash
cd build && make clean && make
```

---

## Documentation

API documentation is generated with [Doxygen](https://www.doxygen.nl) from annotated comments in `main.c`, `crypto_dsa.h`, and `crypto_dsa.c`.

```bash
# Generate HTML documentation
doxygen Doxyfile

# Open in browser (Linux)
xdg-open docs/html/index.html
```

---

## Usage

```
signing_tool <sign|verify> <binary_file>
```

Exactly two positional arguments are required. The first selects the operating mode; the second is the path to the target file. No flags or options are supported.

---

### `sign` — produce a signed binary

```bash
./build/signing_tool sign <binary_file>
```

**What it does:**

1. Reads the raw binary at `<binary_file>` into memory.
2. Loads the Ed25519 key pair from `keys/keys.txt`. If the file does not exist, a new key pair is generated from a `getrandom(2)` seed and persisted automatically.
3. Computes a 64-byte EdDSA signature over the full binary content.
4. Assembles the 72-byte footer: `magic || file_length || signature`.
5. Writes the original binary followed by the footer to `<binary_file>.sign`.

**Example:**

```bash
./build/signing_tool sign build/firmware.bin
```

```
binary file: build/firmware.bin
Keys file found, read keys...
Generating signed binary file
```

**Output file:** `build/firmware.bin.sign`
**Output size:** `sizeof(firmware.bin) + 72 bytes`

> [!NOTE]
> The original `.bin` file is never modified. The `.bin.sign` output is a new file.

---

### `verify` — validate a signed binary

```bash
./build/signing_tool verify <signed_file>
```

**What it does:**

1. Reads `<signed_file>` into memory.
2. Extracts the last 72 bytes as the `signed_file_footer_t` struct.
3. Validates the `magic` field against `0x424F4F54`. Aborts immediately if the magic does not match.
4. Loads the public key from `keys/keys.txt`.
5. Calls `crypto_dsa_verify` over the first `file_length` bytes of the file, using the extracted signature and the loaded public key.
6. Prints the result. The file is **not modified**.

**Example — valid signature:**

```bash
./build/signing_tool verify build/firmware.bin.sign
```

```
binary file: build/firmware.bin.sign
Keys file found, read keys...
Signature is valid
```

**Example — invalid signature** (tampered binary, wrong key, or corrupted footer):

```bash
./build/signing_tool verify build/firmware.bin.sign
```

```
binary file: build/firmware.bin.sign
Keys file found, read keys...
Signature not valid
```

**Example — missing or corrupted footer magic:**

```
Invalid file: footer magic number not found
```

> [!IMPORTANT]
> The public key used during verification **must** match the private key used during signing. If `keys/keys.txt` has been regenerated or replaced between the two operations, verification will fail even on an untampered file.

---

### Error conditions

| Condition                      | Output message                                            |
|--------------------------------|-----------------------------------------------------------|
| Wrong number of arguments      | `Usage: signing_tool <sign\|verify> <file>`               |
| Unknown mode string            | `Error: unknown mode '...'. Use 'sign' or 'verify'.`      |
| Input file not readable        | `Failed to read the binary file to sign`                  |
| Output file creation failure   | `Failed to create output file`                            |
| Footer magic mismatch          | `Invalid file: footer magic number not found`             |

---

## Key Management

Keys are stored in `keys/keys.txt` as two newline-separated lowercase hex strings:

```
<128 hex characters>    ← Ed25519 private key (64 bytes: clamped scalar || public key)
<64 hex characters>     ← Ed25519 public key  (32 bytes: compressed Edwards point)
```

### Lifecycle

```
First run
  │
  ├─ keys/keys.txt not found
  │     └─ getrandom(2) → 32-byte random seed
  │           └─ crypto_eddsa_key_pair(sk, pk, seed)
  │                 └─ hex-encode and write to keys/keys.txt
  │
Subsequent runs
  │
  └─ keys/keys.txt exists
        └─ read and hex-decode into sk / pk buffers
```

File existence is checked with `stat(2)` rather than `fopen`, avoiding a TOCTOU race on certain filesystems.

### Resetting the key pair

```bash
rm keys/keys.txt
```

A new key pair is generated automatically on the next invocation.

> [!WARNING]
> All binaries signed with the old key will **fail verification** after a reset. The new public key will not match any previously computed signature.

---

## Cryptography

### Monocypher v4.0.1

[Monocypher](https://monocypher.org) is a small, portable, audited cryptographic library written in C99. It is distributed as a single `.c` / `.h` pair with zero dependencies beyond the C standard library — making it an ideal cryptographic backend for cross-compiled firmware tools and bare-metal targets where linking against OpenSSL or libsodium is impractical or undesirable.

| Attribute          | Detail                                                                              |
|--------------------|-------------------------------------------------------------------------------------|
| **Language**       | C99, no compiler extensions required                                                |
| **Distribution**   | Single `.c` / `.h` pair — drop-in, no build configuration                          |
| **License**        | BSD-2-Clause and CC0-1.0 (dual)                                                    |
| **Audits**         | Cure53 (2019), Trail of Bits (2022)                                                |
| **Portability**    | No OS dependencies; suitable for bare-metal and RTOS targets                       |

This project uses only the **EdDSA subset** of Monocypher:

| Function                                    | Purpose                                                               |
|---------------------------------------------|-----------------------------------------------------------------------|
| `crypto_eddsa_key_pair(sk, pk, seed)`       | Derive 64-byte private key and 32-byte public key from a 32-byte seed |
| `crypto_eddsa_sign(sig, sk, msg, len)`      | Produce a 64-byte deterministic EdDSA signature                       |
| `crypto_eddsa_check(sig, pk, msg, len)`     | Return `0` if valid, non-zero otherwise                               |

### Algorithm — EdDSA over Curve25519 + BLAKE2b

Monocypher implements EdDSA (RFC 8032) over the **twisted Edwards form of Curve25519**, with one deliberate deviation from the standard: **BLAKE2b replaces SHA-512** as the internal hash function. BLAKE2b is faster than SHA-512 on most architectures and provides a wider security margin against length-extension attacks. The two variants are not wire-compatible with standard Ed25519.

| Property                   | Detail                                                              |
|----------------------------|---------------------------------------------------------------------|
| Signature type             | EdDSA (deterministic) — no RNG required at signing time            |
| Underlying curve           | Curve25519 (twisted Edwards form)                                   |
| Hash function              | BLAKE2b — 512-bit internal, 256-bit output                         |
| Security level             | ~128-bit equivalent (Curve25519 ECDLP hardness)                    |
| Nonce-reuse safety         | Deterministic nonce; immune to nonce-reuse attacks affecting ECDSA  |
| Constant-time              | Branchless scalar multiplication — no timing side-channels          |
| Private key size           | 64 bytes (clamped scalar ‖ public key)                             |
| Public key size            | 32 bytes (compressed Edwards point)                                 |
| Signature size             | 64 bytes (`R ‖ S`)                                                  |

---

## Security Considerations

> [!WARNING]
> `keys/keys.txt` contains the **private key in plaintext**. This file must never be committed to version control.

```bash
# Restrict access immediately after generation
chmod 600 keys/keys.txt
```

Add to `.gitignore`:

```
keys/keys.txt
```

**Additional considerations:**

- **No key encryption at rest.** Keys are stored as raw hex. For production deployments, consider wrapping the key file with a password-derived encryption scheme (e.g., AES-GCM keyed by Argon2) or storing the seed in a hardware security module (HSM) or secure enclave.

- **Key reuse across binaries.** The same key pair is used for every signing operation. EdDSA is safe to reuse (deterministic, no per-signature randomness), but a compromised private key invalidates the entire signing chain — all previously issued signatures remain technically valid, and new fraudulent signatures can be produced.

- **No confidentiality.** The footer and signature are appended in plaintext. The signature guarantees **integrity** and **authenticity**, not confidentiality. Encrypt the binary separately if confidentiality is required.

- **`file_length` is trusted on verify.** The verification path uses the `file_length` field from the footer to determine the signed message boundary. A tampered `file_length` does not bypass verification — the signature covers only the original byte range, so any mismatch causes `crypto_dsa_verify` to return invalid.

- **Cross-environment key distribution.** If signing and verification occur on different machines, `keys/keys.txt` must be securely transferred to the verifying host. There is no embedded certificate or trust chain — the public key is implicitly trusted by virtue of its presence in the key file.

---

## License

Monocypher is dual-licensed **BSD-2-Clause / CC0-1.0**. Full license text is embedded at the top of `Libraries/Src/monocypher.c`.
