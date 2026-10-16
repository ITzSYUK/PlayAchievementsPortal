# Mbed TLS transport experiment

This is a standalone, credential-free experiment. It does not implement the
RetroAchievements client transport, parse API bodies or register achievements.
Mbed TLS 3.6.7 and the public CA bundle are pinned in `dependencies.lock.json`.
Upstream source files are unmodified; `psp_tls_config.h` is shared by host and
PSP builds. TLS 1.2, authenticated ECDHE RSA/ECDSA, AES-GCM/ChaCha20-Poly1305;
certificate, hostname, expiry and key usage validation is REQUIRED.

Host verification from the repository root in Ubuntu WSL:

```sh
make tls-test
make -s -C experiments/tls -j4 test BUILD=../../build/tls-sanitized \
  'CFLAGS=-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' \
  'LDFLAGS=-fsanitize=address,undefined'
```

Tests require the existing Python `cryptography` package. They create a
temporary CA and RSA/ECDSA server certificates, run loopback servers and test
valid chains, wrong names, expired/untrusted certificates, no entropy,
malformed/oversized CA files, timeouts and bounded HTTP headers. Ephemeral
test keys are removed by TemporaryDirectory and never packaged.

Probe 0.2 uses `MBEDTLS_PLATFORM_TIME_ALT`: the core installs an explicit UTC
callback, independently of the monotonic deadline clock. Host uses `time(NULL)`;
PSP uses `sceRtcGetCurrentTick`, the epoch anchor from
`sceRtcGetTick(1970-01-01 UTC)`, and `sceRtcGetTickResolution`. No firmware
`time_t*` is passed to RTC, avoiding 32/64-bit ABI assumptions. The conversion
checks underflow, zero resolution, int64 overflow and a time_t round-trip.
No RTC error falls back to libc time or uptime. Actual RTC UTC and the UTC
passed to TLS are logged; libc time is logged only for comparison.

The owner's 0.1 logs reported libc Unix seconds 46616/46814 (1970-01-01), and
both valid servers failed with X.509 FUTURE 0x0200. The user's console date was
checked; the probe's libc clock path was unsuitable. Regression tests reproduce
that FUTURE rejection using 46616, verify unavailable UTC fails before sending
HTTP, and retain hostname, expiry and trust checks with correct UTC. Separate
RTC conversion tests include an independent 2026 reference and dates beyond
2038. PSP 0.2 still needs a hardware run to confirm RTC results and HTTPS.

The core owns all Mbed TLS contexts and temporarily installs a counted
allocator (2 MiB total including allocation headers). Only one invocation may
run at a time. It returns after complete HTTP headers, reads at most 8 KiB of
plaintext including any body fragment, and does not claim a complete response
body. HTTP status and certificate verification are distinct results.

PSP: DNS timeout 7 seconds/one attempt, TCP connect 7 seconds, Wi-Fi 30 seconds;
handshake, send and header read each have a monotonic 45-second deadline.
Nonblocking socket BIOs and bounded polls keep network waits on the worker.
A single expensive cryptographic operation or firmware DNS call can overrun a
between-call deadline; no strict wall-time or in-game timing guarantee is made.
CA file max 256 KiB. No redirects, credentials, HTTP downgrade or proxy.

PSP entropy comes from a small, temporary kernel PRX using `semaphore` KIRK
command 14, four 20-byte results. `kuKernelLoadModule` loads it through ARK's
KUBridge. Its start argument contains a pointer to a fixed 84-byte user output
structure; only aligned base user RAM addresses are accepted, no command or
length can be supplied. Seed bytes are copied only after every call succeeds;
all-zero or consecutively repeated blocks fail closed. Buffers are erased,
the helper stopped/unloaded, then 48 bytes seed CTR_DRBG. The remaining seed is
erased at cleanup; reseeding after exhausting the buffer fails closed.

This is a hardware cryptographic PRNG API candidate, not a validation of the
firmware's entropy properties. The owner's two 0.1 runs passed
load/start/KIRK/stop/unload. No SDK `getentropy()` fallback: reference
PSPSDK libcglue seeds MT19937 using `time(NULL)`, unsuitable for TLS.

References (read as API documentation, no CFW source incorporated):
- [PSPSDK entropy implementation](https://github.com/pspdev/pspsdk/blob/e2b2b313d4175823a263c2c6cf175311e824eb77/src/libcglue/glue.c)
- [ARK5 KUBridge implementation](https://github.com/PSP-Arkfive/ARK-5/blob/a9b74547c0d1785cc77981462cf095bd5a76434f/SystemControl/src/kubridge.c)
- [ARK5 export table](https://github.com/PSP-Arkfive/ARK-5/blob/a9b74547c0d1785cc77981462cf095bd5a76434f/SystemControl/exports.exp)
- [PSPDEV NID database](https://github.com/pspdev/psp-ghidra-scripts/blob/master/ppsspp_niddb.xml)

These source revisions do not identify the owner's installed ARK5 build.
EBOOT and helper compile with `-Werror`. User testing: `docs/PSP_PROBES.md`.
