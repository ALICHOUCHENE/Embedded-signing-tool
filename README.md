# Binary Signing Tool

A lightweight, self-contained C utility for cryptographically signing and verifying binary files using EdDSA (Ed25519 / BLAKE2b). Designed for embedded and desktop workflows where firmware or binary integrity must be guaranteed before deployment or execution.

---

## Table of Contents

- [Overview](#overview)
- [Project Structure](#project-structure)
- [Architecture](#architecture)
- [Signed File Format](#signed-file-format)
- [Dependencies](#dependencies)
- [Build](#build)
- [Documentation](#documentation)
- [Usage](#usage)
- [Key Management](#key-management)
- [Cryptography](#cryptography)
- [Security Considerations](#security-considerations)
- [License](#license)

---

## Overview

The tool operates in two modes selected at runtime via a CLI argument:

| Mode     | Input                    | Output                        |
|----------|--------------------------|-------------------------------|
| `sign`   | Raw binary (`*.bin`)     | Signed binary (`*.bin.sign`)  |
| `verify` | Signed binary (`*.sign`) | Validation result on stdout   |

Signing appends a compact **72-byte footer** to the original binary containing a magic identifier, the original file length, and a 64-byte EdDSA signature. Verification extracts the footer, validates the magic number, and checks the signature against the stored public key — without modifying the file.

Keys are automatically generated on first run using `getrandom(2)` and persisted to `keys/keys.txt` as hex-encoded strings for reuse across invocations.

---

## Project Structure

```
signing_tool/
├── CMakeLists.txt                   # Build system
├── Doxyfile                         # Doxygen configuration
├── keys/
│   └── keys.txt                     # Auto-generated key storage (hex-encoded, gitignored)
├── Core/
│   └── Src/
│       └── main.c                   # Entry point: CLI parsing, sign/verify orchestration
├── Middleware/
│   ├── Inc/
│   │   └── crypto_dsa.h             # Public DSA API: types, constants, function declarations
│   └── Src/
│       └── crypto_dsa.c             # DSA implementation: key I/O, sign, verify
└── Libraries/
    ├── Inc/
    │   └── monocypher.h             # Monocypher v4.0.1 — single-header interface
    └── Src/
        └── monocypher.c             # Monocypher v4.0.1 — full implementation
```

---

## Architecture

The codebase is organized in three layers, each with a single responsibility:

```
┌─────────────────────────────────────────────────┐
│                   main.c                        │
│  CLI parsing · file I/O · footer assembly       │
│  _sign_binary_file()  _verify_signature()       │
│  _generate_signed_binary_file()                 │
└────────────────────┬────────────────────────────┘
                     │ calls
┌────────────────────▼────────────────────────────┐
│               crypto_dsa.c / .h                 │
│  Key lifecycle · hex encoding · POSIX file I/O  │
│  crypto_dsa_generate_keys()                     │
│  crypto_dsa_sign()  crypto_dsa_verify()         │
└────────────────────┬────────────────────────────┘
                     │ calls
┌────────────────────▼────────────────────────────┐
│              monocypher.c / .h                  │
│  EdDSA primitives (Curve25519 + BLAKE2b)        │
│  crypto_eddsa_key_pair()                        │
│  crypto_eddsa_sign()  crypto_eddsa_check()      │
└─────────────────────────────────────────────────┘
```

`main.c` handles all file I/O and CLI logic. `crypto_dsa` is the middleware layer that manages key persistence and wraps the Monocypher primitives behind a stable, project-specific API. `monocypher` is a vendored, unmodified cryptographic library and is never called directly from application code.

---

## Signed File Format

A signed file is a byte-for-byte copy of the original binary followed by a packed 72-byte footer:

```
┌──────────────────────────────────────────────────────────────────┐
│                     Original binary data                         │
│                        (N bytes)                                 │
├──────────────────┬──────────────────┬────────────────────────────┤
│  magic (4 bytes) │ file_len (4 bytes)│   signature (64 bytes)    │
│   0x424F4F54     │       N          │       R ‖ S (EdDSA)        │
│     "BOOT"       │                  │                            │
└──────────────────┴──────────────────┴────────────────────────────┘
                        signed_file_footer_t (72 bytes, packed)
```

| Field         | Type       | Size     | Description                                          |
|---------------|------------|----------|------------------------------------------------------|
| `magic`       | `uint32_t` | 4 bytes  | `0x424F4F54` ("BOOT") — identifies a valid footer    |
| `file_length` | `uint32_t` | 4 bytes  | Length of the original binary, used as message size  |
| `signature`   | `uint8_t[64]` | 64 bytes | EdDSA signature over the original binary bytes    |

The footer is defined as `__attribute__((packed))` to prevent compiler-inserted padding. The signature covers only the original binary bytes (bytes `0` to `file_length - 1`), not the footer itself.

On verification, the footer is extracted from the last 72 bytes of the file, the magic number is checked, and `crypto_dsa_verify` is called with `file_length` as the message size — ensuring that any modification to the binary body or the footer length field would fail verification.

---

## Dependencies

| Dependency | Version  | Purpose                                | Notes                              |
|------------|----------|----------------------------------------|------------------------------------|
| CMake      | ≥ 3.0    | Build system                           | —                                  |
| GCC        | any C99  | Compiler                               | Clang also works                   |
| Linux      | any      | `getrandom(2)` for seed generation     | Requires `sys/random.h`            |
| Monocypher | 4.0.1    | EdDSA / BLAKE2b cryptographic library  | Vendored — no install needed       |
| Doxygen    | ≥ 1.9    | API documentation generation           | Optional, only for docs            |

No external libraries need to be installed. Monocypher is bundled as a single `.c` / `.h` pair under `Libraries/`.

---

## Build

### Standard build

```bash
mkdir build && cd build
cmake ..
make
```

The compiled binary is placed at `build/signing_tool`.

### Verbose build (show compiler commands)

```bash
make VERBOSE=1
```

### Clean rebuild

```bash
cd build
make clean
make
```

### Out-of-tree build in a custom directory

```bash
mkdir -p /tmp/signing_build
cmake -S . -B /tmp/signing_build
cmake --build /tmp/signing_build
```

---

## Documentation

API documentation is generated with [Doxygen](https://www.doxygen.nl) from the Doxygen-annotated comments in `Core/Src/main.c`, `Middleware/Inc/crypto_dsa.h`, and `Middleware/Src/crypto_dsa.c`.

### Install Doxygen

```bash
sudo apt install doxygen        # Debian / Ubuntu
sudo dnf install doxygen        # Fedora / RHEL
brew install doxygen            # macOS
```

### Generate the documentation

Run from the project root (where `Doxyfile` is located):

```bash
doxygen Doxyfile
```

Output is written to `docs/html/`. Open the entry point in a browser:

```bash
xdg-open docs/html/index.html   # Linux
open docs/html/index.html        # macOS
```

### Doxyfile highlights

| Setting              | Value                              | Effect                                        |
|----------------------|------------------------------------|-----------------------------------------------|
| `INPUT`              | `Core/Src`, `Middleware/Inc/Src`   | Directories scanned for annotated source      |
| `EXTRACT_ALL`        | `YES`                              | Documents all symbols, not just public ones   |
| `EXTRACT_STATIC`     | `YES`                              | Includes static (file-local) functions        |
| `GENERATE_LATEX`     | `NO`                               | Skips LaTeX output, HTML only                 |
| `OUTPUT_DIRECTORY`   | `docs`                             | Root output directory                         |

---

## Usage

```
signing_tool <sign|verify> <binary_file>
```

### Sign a binary

```bash
./build/signing_tool sign path/to/firmware.bin
```

Expected output:

```
binary file: path/to/firmware.bin
Keys file found, read keys...
Generating signed binary file
```

The signed output is written to `path/to/firmware.bin.sign`. Its size is the original file size plus 72 bytes (the footer).

### Verify a signed binary

```bash
./build/signing_tool verify path/to/firmware.bin.sign
```

Expected output on success:

```
binary file: path/to/firmware.bin.sign
Keys file found, read keys...
Signature is valid
```

Expected output on failure (tampered file or wrong key):

```
binary file: path/to/firmware.bin.sign
Keys file found, read keys...
Signature not valid
```

If the file does not contain a valid footer magic number:

```
Invalid file: footer magic number not found
```

### Error handling summary

| Condition                          | Output message                              | Exit |
|------------------------------------|---------------------------------------------|------|
| Wrong number of arguments          | `Usage: signing_tool <sign\|verify> <file>` | `-1` |
| Unknown mode string                | `Error: unknown mode '...'. Use 'sign' or 'verify'.` | `-1` |
| Input file not found               | `Failed to read the binary file to sign`    | —    |
| Memory allocation failure          | *(silent NULL return)*                      | —    |
| Footer magic mismatch              | `Invalid file: footer magic number not found` | —  |
| Output file creation failure       | `Failed to create output file`              | —    |

---

## Key Management

Keys are stored in `keys/keys.txt` as two newline-separated hex-encoded strings:

```
line 1:  private key — 128 hex characters (64 bytes)
line 2:  public key  —  64 hex characters (32 bytes)
```

### Lifecycle

```
First run
   │
   ├─ keys/keys.txt not found
   │      └─ getrandom(2) generates 32-byte seed
   │         └─ crypto_eddsa_key_pair() derives private + public key
   │            └─ keys written to keys/keys.txt as hex strings
   │
Subsequent runs
   │
   └─ keys/keys.txt found
          └─ keys loaded and decoded from hex
```

The existence check uses POSIX `stat(2)` (`_file_exists()`), which avoids a race condition that `fopen` for reading would introduce on some filesystems.

### Resetting the key pair

```bash
rm keys/keys.txt
```

On the next run a new key pair is generated automatically. **All files signed with the previous key will fail verification** after this operation, since the new public key will not match the stored signatures.

> **Important:** `keys/keys.txt` must be kept consistent between the signing and verification invocations. If signing and verification are performed on different machines or environments, the key file must be copied alongside the signed binary.

---

## Cryptography

### Library: Monocypher v4.0.1

[Monocypher](https://monocypher.org) is a self-contained, portable cryptographic library written in C. It is dual-licensed under BSD-2-Clause and CC0-1.0. The entire library is a single `.c` / `.h` pair with no dependencies beyond the C standard library, making it well-suited for bare-metal and embedded targets.

This project uses only the EdDSA primitives:

| Monocypher function                             | Purpose                                                       |
|-------------------------------------------------|---------------------------------------------------------------|
| `crypto_eddsa_key_pair(sk, pk, seed)`           | Derive 64-byte private key and 32-byte public key from a 32-byte seed |
| `crypto_eddsa_sign(sig, sk, msg, len)`          | Produce a 64-byte deterministic signature                     |
| `crypto_eddsa_check(sig, pk, msg, len)`         | Return `0` if signature is valid, non-zero otherwise          |

### Algorithm: EdDSA over Curve25519 + BLAKE2b

EdDSA (Edwards-curve Digital Signature Algorithm, RFC 8032) as implemented by Monocypher operates over the twisted Edwards form of Curve25519. It deviates from standard Ed25519 by using **BLAKE2b** as its hash function instead of SHA-512. This is an intentional Monocypher design choice for improved performance and a larger security margin.

#### Key derivation

A 32-byte random seed (from `getrandom(2)`) is passed to `crypto_eddsa_key_pair`. Internally:

1. BLAKE2b hashes the seed to produce a 64-byte value.
2. The first 32 bytes are clamped (bits 0, 1, 2, 255 cleared; bit 254 set) to form the scalar `a`.
3. The last 32 bytes become a deterministic signing prefix.
4. The public key `A = [a]B`, where `B` is the base point of the curve.

The private key written to disk is the full 64-byte Monocypher layout: `scalar || public_key`.

#### Signing

Given message `M` and private key `(a, prefix, A)`:

```
r  = BLAKE2b(prefix ‖ M) mod L        # deterministic nonce scalar
R  = [r]B                              # nonce point (32 bytes)
h  = BLAKE2b(R ‖ A ‖ M) mod L         # challenge scalar
S  = (r + h·a) mod L                  # response scalar (32 bytes)

signature = R ‖ S                      # 64 bytes total
```

`L` is the prime order of the base point. Because `r` is derived deterministically, EdDSA requires no external randomness at signing time and is immune to nonce-reuse attacks that affect ECDSA.

#### Verification

Given message `M`, public key `A`, and signature `(R, S)`:

```
h = BLAKE2b(R ‖ A ‖ M) mod L
check: [S]B == R + [h]A
```

The signature is valid if and only if the group equation holds.

#### Security properties

| Property                     | Detail                                                                  |
|------------------------------|-------------------------------------------------------------------------|
| Deterministic signatures     | No RNG at signing time; eliminates nonce-reuse vulnerabilities          |
| 128-bit security level       | Curve25519 provides ~128-bit equivalent security                        |
| Collision resistance         | BLAKE2b offers 256-bit output with strong preimage resistance           |
| Constant-time implementation | Monocypher uses branchless scalar multiplication to prevent timing side-channels |
| Unforgeability               | Relies on the hardness of ECDLP on Curve25519                          |

### DSA middleware layer (`crypto_dsa`)

`Middleware/Src/crypto_dsa.c` wraps the Monocypher primitives behind a stable, project-specific API and handles all key persistence logic.

#### Public API

```c
/* Generate a new key pair or load an existing one from keys/keys.txt */
void crypto_dsa_generate_keys(crypto_dsa_private_key_t private_key,
                              crypto_dsa_public_key_t  public_key);

/* Sign a message — thin wrapper over crypto_eddsa_sign */
void crypto_dsa_sign(crypto_dsa_private_key_t  private_key,
                     const uint8_t             *data,
                     size_t                     message_size,
                     crypto_dsa_signature_t     signature);

/* Verify a signature — returns true if valid, false otherwise */
bool crypto_dsa_verify(crypto_dsa_public_key_t  public_key,
                       const uint8_t            *data,
                       size_t                    message_size,
                       crypto_dsa_signature_t    signature);
```

#### Type definitions and key sizes

```c
typedef uint8_t crypto_dsa_private_key_t[CRYPTO_DSA_PRIVATE_KEY_LENGTH];  // 64 bytes
typedef uint8_t crypto_dsa_public_key_t [CRYPTO_DSA_PUBLIC_KEY_LENGTH];   // 32 bytes
typedef uint8_t crypto_dsa_signature_t  [CRYPTO_DSA_SIGNATURE_LENGTH];    // 64 bytes
```

| Constant                       | Value    | Description                                    |
|--------------------------------|----------|------------------------------------------------|
| `CRYPTO_DSA_PRIVATE_KEY_LENGTH`| 64 bytes | Ed25519 private key (clamped scalar + public key) |
| `CRYPTO_DSA_PUBLIC_KEY_LENGTH` | 32 bytes | Curve25519 point (compressed Edwards coordinates) |
| `CRYPTO_DSA_SIGNATURE_LENGTH`  | 64 bytes | EdDSA signature: `R ‖ S`                       |

#### Internal helpers (static, file-local)

| Function                  | Purpose                                                         |
|---------------------------|-----------------------------------------------------------------|
| `_file_exists()`          | `stat(2)`-based file existence check                           |
| `_generate_dsa_keys()`    | Calls `getrandom(2)` for seed, then `crypto_eddsa_key_pair()`  |
| `_convert_hex_to_str()`   | Binary → lowercase hex string (`sprintf` loop)                 |
| `_convert_str_to_hex()`   | Hex string → binary (`sscanf` with `%2hhx` format)             |
| `_save_keys_to_file()`    | Writes hex-encoded private and public keys on separate lines   |
| `_load_keys_from_file()`  | Reads and decodes keys from `keys/keys.txt`                    |
| `_generate_new_keys()`    | Orchestrates generation + file write on first run              |

---

## Security Considerations

- **Private key exposure:** `keys/keys.txt` contains the private key in plaintext hex. It must not be committed to version control (add it to `.gitignore`) and should be protected with appropriate filesystem permissions (`chmod 600 keys/keys.txt`).

- **Key reuse across binaries:** The same key pair is reused for every signing operation. This is acceptable for EdDSA (which is deterministic and safe to reuse) but means that compromising the private key invalidates the entire signing chain.

- **No key derivation password:** Keys are stored unencrypted. For production use, consider wrapping `keys/keys.txt` with a password-based encryption scheme or storing the seed in a hardware security module (HSM).

- **Footer is not encrypted:** The footer and signature are appended in plaintext. The signature guarantees integrity and authenticity but not confidentiality.

- **`file_length` is trusted on verify:** The verification path trusts the `file_length` field in the footer to determine the message boundary. A tampered `file_length` would cause `crypto_dsa_verify` to operate on the wrong byte range and return invalid — so this does not represent a bypass vector, but it means verification correctly rejects any footer modification.

---

## License

Monocypher is dual-licensed BSD-2-Clause / CC0-1.0. See `Libraries/Src/monocypher.c` for the full license text.
