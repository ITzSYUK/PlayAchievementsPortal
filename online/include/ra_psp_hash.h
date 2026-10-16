#ifndef RA_PSP_HASH_H
#define RA_PSP_HASH_H

/* Experimental disc0/extracted-file counterpart of rc_hash_psp's ISO branch.
 * It cannot prove the files are the original encrypted bytes of a game image.
 * Validate against the official ISO hash before using it to identify a game. */
int ra_psp_hash_disc_files(char hash[33], const char* param_sfo,
    const char* eboot_bin);

#endif
