#ifndef RA_GAME_CODE_AUDIT_H
#define RA_GAME_CODE_AUDIT_H
#include "ra_game_memory.h"
#define RA_AUDIT_IMPORTS 16u
#define RA_AUDIT_CALLS 32u
#define RA_AUDIT_REFS 8u
#define RA_METHOD_SITES 32u
typedef struct {uint32_t table_pc,method_pc,call_pc;} ra_method_site;
typedef struct {unsigned count,total,chains,scanned_words;ra_method_site sites[RA_METHOD_SITES];} ra_method_audit;
/* Lexical LW +0x14 -> JALR through the loaded register within eight words.
 * Optional preceding LW +0x10 identifies an object-table chain candidate.
 * No register lifetime, execution or frame proof; windows must be inspected.
 * Scan all text even when the retained site limit is reached. */
int ra_game_method_calls(ra_method_audit*,const ra_game_map*,uint32_t,uint32_t,
    ra_psp_platform_read_t,void*);
typedef struct { uint32_t upper_pc,lower_pc,reg,operation; } ra_address_pair;
#define RA_TABLE_WORDS 16u
enum {RA_TABLE_ZERO,RA_TABLE_CODE,RA_TABLE_GAME_ADDRESS,RA_TABLE_OPAQUE};
typedef struct {unsigned count;uint32_t values[RA_TABLE_WORDS],kinds[RA_TABLE_WORDS];} ra_table_probe;
typedef struct {
    unsigned pairs,total_pairs,pointers,total_pointers;
    ra_address_pair code[RA_AUDIT_REFS];
    uint32_t slots[RA_AUDIT_REFS];
} ra_address_refs;
typedef struct { uint32_t nid, stub; } ra_audit_import;
typedef struct { uint32_t pc, instruction, import_index; } ra_audit_call;
typedef struct {
    unsigned imports, calls, total_calls, libraries;
    ra_audit_import targets[RA_AUDIT_IMPORTS];
    ra_audit_call sites[RA_AUDIT_CALLS];
} ra_code_audit;
/* Read-only scan. Every metadata/text read must fit a loaded game segment.
 * J/JAL matches are static candidates, never proof of executed game frames. */
int ra_game_code_audit(ra_code_audit*, const ra_game_map*, uint32_t text, uint32_t text_size,
    uint32_t stubs, uint32_t stub_size, ra_psp_platform_read_t, void*);
uint32_t ra_mips_jump_target(uint32_t pc, uint32_t instruction);
/* Static direct J/JAL candidates only, capped by RA_AUDIT_CALLS; indirect
 * calls and data-flow are deliberately not inferred. */
int ra_game_find_calls(ra_code_audit*,const ra_game_map*,uint32_t text,uint32_t text_size,
    uint32_t target,ra_psp_platform_read_t,void*);
/* Lexical LUI + ADDIU/ORI/LW/SW candidates within 7 instructions, not data-flow
 * proof: intervening writes/branches are left for inspection of CODE windows.
 * Pointer matches are aligned words in allowed segments, never dereferenced. */
int ra_mips_address_pair(uint32_t upper,uint32_t lower,uint32_t target,unsigned* reg);
int ra_game_address_refs(ra_address_refs*,const ra_game_map*,uint32_t text,uint32_t text_size,
    uint32_t target,int scan_pointers,ra_psp_platform_read_t,void*);
/* Up to 16 fixed words, no pointer dereference. guard_index==RA_TABLE_WORDS
 * disables the exact-word guard. -1 bounds, -2 guard mismatch, -4 short/read error.
 * Caller must never log values classified OPAQUE or GAME_ADDRESS. */
int ra_game_table_probe(ra_table_probe*,const ra_game_map*,uint32_t text,uint32_t text_size,
    uint32_t table,unsigned words,unsigned guard_index,uint32_t expected,ra_psp_platform_read_t,void*);
/* Conservative lexical boundary candidate, not a function/CFG proof.
 * Search <=512 bytes backward for aligned negative ADDIU sp + SW ra
 * within its next 32 bytes, and never cross a JR ra. No candidate =>0.
 * Returns 1 candidate, -1 invalid input, -4 failed/short read. */
int ra_game_function_start(uint32_t* start,const ra_game_map*,uint32_t text,uint32_t text_size,
    uint32_t call_pc,ra_psp_platform_read_t,void*);
#endif
