# Public CA bundle

Unmodified Mozilla-derived PEM bundle from curl, dated 2026-09-25, renamed
`cacert.crt` to distinguish public trust anchors from ignored private `.pem`
files. URL and the SHA-256 of the entire downloaded file are pinned in
`dependencies.lock.json`; verified against curl's published checksum.

Source: https://curl.se/ca/cacert-2026-09-25.pem
Checksum: https://curl.se/ca/cacert-2026-09-25.pem.sha256
Extraction details: https://curl.se/docs/caextract.html
License: MPL-2.0, text in `LICENSE.txt`.

Contains 121 CA certificates, not account credentials or a client private key.
Firefox trust constraints outside the certificate file are not reproduced by
this bundle. This diagnostic uses Mbed TLS chain, hostname, validity and key
usage checks; it does not implement browser revocation checks or OCSP.
