#ifndef RA_ENTROPY_REQUEST_H
#define RA_ENTROPY_REQUEST_H
#define RA_ENTROPY_BYTES 80
/* module_start receives a COPY of this argument; output points to user RAM.
 * Only the fixed size below is accepted. No arbitrary command or length. */
typedef struct { unsigned char bytes[RA_ENTROPY_BYTES]; int status; } ra_entropy_output;
typedef struct { ra_entropy_output* output; } ra_entropy_request;
#endif
