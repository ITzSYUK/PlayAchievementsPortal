#ifndef RA_KERNEL_GAME_AUDIT_H
#define RA_KERNEL_GAME_AUDIT_H
#include "game_code_audit.h"
/* PSPSDK exports this NID; ABI cross-checked against uOFW sysmem/memory.c.
 * Query returns partition UID, not necessarily canonical partition number. */
int sceKernelQueryMemoryInfo(unsigned, int*, int*);
static unsigned audit_read_bytes;
static uint32_t audit_read(uint32_t address,uint8_t* bytes,uint32_t size,void* context)
{
    const ra_game_map* map=context;
    uint32_t i;
    const volatile uint8_t* p=(const volatile uint8_t*)(uintptr_t)address;
    if(stopping || !control || control->stop || !ra_game_map_contains(map,address,size))return 0;
    for(i=0;i<size;++i)bytes[i]=p[i];
    audit_read_bytes+=size;
    if(audit_read_bytes>=65536u){audit_read_bytes=0;sceKernelDelayThread(1000);}
    return size;
}
static void audit_block(const SceModule* game,unsigned second)
{
    int partition=-1,memory=-1;
    int rc=sceKernelQueryMemoryInfo(0x0979E4AC,&partition,&memory);
    /* No contents, names, kernel pointers or additional ranges are read. */
    ra_probe_log_printf(&logger,"GAME_BLOCK second=%u address=0x0979E4AC query=0x%08X partition_uid=0x%08X block_uid=0x%08X same_game_block=%u same_control_block=%u read=DENIED\n",
        second,(unsigned)rc,(unsigned)partition,(unsigned)memory,
        rc==0 && memory>0 && memory==game->mem_id,rc==0 && memory>0 && memory==block);
}
#define RA_TABLE_CODE_BUDGET 3072u
typedef struct {unsigned budget,windows;uint32_t first[40];} ra_table_audit_state;
static void table_window(const SceKernelModuleInfo* info,uint32_t first,uint32_t size,
    const char* kind,ra_table_audit_state* state)
{
    uint32_t end=info->text_addr+info->text_size,pc,words,wanted;unsigned i;
    if(first%4 || !size || size%4 || size>512 || first<info->text_addr || first>=end){
        ra_probe_log_printf(&logger,"GAME_TABLE_WINDOW kind=%s first=0x%08X result=-1\n",kind,first);return;
    }
    for(i=0;i<state->windows;++i)if(state->first[i]==first)return;
    if(state->windows==40){ra_probe_log_printf(&logger,"GAME_TABLE_WINDOW kind=%s reason=window_limit\n",kind);return;}
    state->first[state->windows++]=first;
    if(size>end-first)size=end-first;
    wanted=size/4;words=wanted;if(words>state->budget)words=state->budget;
    ra_probe_log_printf(&logger,"GAME_TABLE_WINDOW kind=%s first=0x%08X words=%u truncated=%u\n",kind,first,words,words<wanted);
    end=first+words*4;
    for(pc=first;pc<end && !logger.error && !stopping;pc+=4){
        uint8_t b[4];uint32_t word;
        if(audit_read(pc,b,4,&control->game_map)!=4){
            ra_probe_log_printf(&logger,"GAME_TABLE_READ_ERROR pc=0x%08X\n",pc);return;
        }
        word=b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;
        --state->budget;ra_probe_log_printf(&logger,"CODE %08X %08X\n",pc,word);
    }
}
static int table_fields(const SceKernelModuleInfo* info,const char* name,uint32_t address,
    unsigned words,unsigned guard,uint32_t expected,ra_table_probe* table)
{
    static const char* const kinds[]={"zero","code","game_address","opaque"};
    unsigned i;
    int rc=ra_game_table_probe(table,&control->game_map,info->text_addr,info->text_size,
        address,words,guard,expected,audit_read,&control->game_map);
    ra_probe_log_printf(&logger,"GAME_TABLE_PROBE name=%s address=0x%08X words=%u guard=%u result=%d span_candidate=1\n",
        name,address,words,guard<RA_TABLE_WORDS,rc);
    if(rc)return rc;
    for(i=0;i<table->count;++i){
        if(table->kinds[i]==RA_TABLE_CODE)
            ra_probe_log_printf(&logger,"GAME_TABLE_FIELD name=%s index=%u kind=code address=0x%08X\n",name,i,table->values[i]);
        else
            ra_probe_log_printf(&logger,"GAME_TABLE_FIELD name=%s index=%u kind=%s value=HIDDEN\n",name,i,kinds[table->kinds[i]]);
    }
    return 0;
}
static void method_context(const SceKernelModuleInfo* info,uint32_t pc,const char* kind,ra_table_audit_state* state)
{
    uint32_t first=pc-info->text_addr>=128?pc-128:info->text_addr;
    table_window(info,first,512,kind,state);
}
#define RA_PAUSE_NODES 12u
#define RA_PAUSE_EDGES 64u
typedef struct {uint32_t address,span;unsigned depth;} ra_pause_node;
typedef struct {
    ra_pause_node nodes[RA_PAUSE_NODES];unsigned count,dropped,edges,total_edges,errors;
} ra_pause_flow;
static int pause_signature(const SceKernelModuleInfo* info,uint32_t pc,uint32_t expected)
{
    uint8_t bytes[4];uint32_t word;int rc=0;
    if(pc%4 || pc<info->text_addr || info->text_size<4 || pc-info->text_addr>info->text_size-4)rc=-1;
    else if(audit_read(pc,bytes,4,&control->game_map)!=4)rc=-4;
    else {word=bytes[0]|(uint32_t)bytes[1]<<8|(uint32_t)bytes[2]<<16|(uint32_t)bytes[3]<<24;
        if(word!=expected)rc=-2;}
    ra_probe_log_printf(&logger,"GAME_PAUSE_GUARD pc=0x%08X result=%d\n",pc,rc);
    return rc;
}
static void pause_add(ra_pause_flow* flow,const SceKernelModuleInfo* info,uint32_t address,unsigned depth,uint32_t span)
{
    unsigned i;
    if(address%4 || address<info->text_addr || address-info->text_addr>=info->text_size)return;
    for(i=0;i<flow->count;++i)if(address>=flow->nodes[i].address && address-flow->nodes[i].address<flow->nodes[i].span)return;
    if(flow->count==RA_PAUSE_NODES){++flow->dropped;return;}
    if(span>info->text_size-(address-info->text_addr))span=info->text_size-(address-info->text_addr);
    flow->nodes[flow->count].address=address;flow->nodes[flow->count].depth=depth;
    flow->nodes[flow->count++].span=span;
}
static void pause_node(ra_pause_flow* flow,const SceKernelModuleInfo* info,unsigned index,ra_table_audit_state* state)
{
    ra_pause_node node=flow->nodes[index];uint32_t pc,offset;unsigned words=0;
    ra_probe_log_printf(&logger,"GAME_PAUSE_NODE index=%u address=0x%08X depth=%u span=%u candidate=1\n",
        index,node.address,node.depth,node.span);
    for(offset=0;offset<node.span;offset+=512){
        uint32_t bytes=node.span-offset;if(bytes>512)bytes=512;
        table_window(info,node.address+offset,bytes,"pause_flow",state);
    }
    for(pc=node.address;pc-node.address<node.span && !logger.error && !stopping;pc+=4){
        uint8_t bytes[4];uint32_t word,target;unsigned op;
        if(audit_read(pc,bytes,4,&control->game_map)!=4)break;
        ++words;word=bytes[0]|(uint32_t)bytes[1]<<8|(uint32_t)bytes[2]<<16|(uint32_t)bytes[3]<<24;
        op=word>>26;if(op!=2 && op!=3)continue;
        target=ra_mips_jump_target(pc,word);++flow->total_edges;
        if(flow->edges<RA_PAUSE_EDGES){
            ++flow->edges;
            ra_probe_log_printf(&logger,"GAME_PAUSE_EDGE pc=0x%08X target=0x%08X operation=%u lexical=1\n",pc,target,op);
        }
        if(node.depth<2)pause_add(flow,info,target,node.depth+1,384);
    }
    if(words!=node.span/4)++flow->errors;
    ra_probe_log_printf(&logger,"GAME_PAUSE_NODE_DONE index=%u scanned_words=%u expected_words=%u\n",index,words,node.span/4);
}
static int pause_callers(const SceKernelModuleInfo* info,uint32_t target,unsigned window_limit,
    unsigned* windows,ra_table_audit_state* state)
{
    ra_code_audit calls;unsigned i;int rc=ra_game_find_calls(&calls,&control->game_map,
        info->text_addr,info->text_size,target,audit_read,&control->game_map);
    ra_probe_log_printf(&logger,"GAME_PAUSE_CALLERS target=0x%08X result=%d retained=%u total=%u window_limit=%u\n",
        target,rc,calls.calls,calls.total_calls,window_limit);
    if(rc)return rc;
    for(i=0;i<calls.calls && !logger.error && !stopping;++i){
        uint32_t owner=0;int boundary=ra_game_function_start(&owner,&control->game_map,
            info->text_addr,info->text_size,calls.sites[i].pc,audit_read,&control->game_map);
        ra_probe_log_printf(&logger,"GAME_PAUSE_CALL target=0x%08X pc=0x%08X owner_candidate=0x%08X boundary_result=%d lexical=1\n",
            target,calls.sites[i].pc,owner,boundary);
        if(i<window_limit && *windows<8){method_context(info,calls.sites[i].pc,"pause_caller",state);++*windows;}
    }
    return logger.error || stopping ? -4 : 0;
}
static void __attribute__((unused)) audit_table_flow(const SceKernelModuleInfo* info)
{
    ra_table_probe descriptor;ra_table_audit_state state={RA_TABLE_CODE_BUDGET,0,{0}};
    ra_pause_flow flow={{{0,0,0}},0,0,0,0,0};unsigned i,caller_windows=0;int guard,rc=-1;
    guard=table_fields(info,"input_descriptor",0x089B4ED0,6,5,0x08842D90,&descriptor);
    if(guard)goto done;
    rc=pause_signature(info,0x08844440,0x27BDFFF0);if(rc)goto done;
    rc=pause_signature(info,0x08842CA0,0x0E2105E2);if(rc)goto done;
    rc=pause_signature(info,0x08842CA8,0x0E211110);if(rc)goto done;
    rc=pause_signature(info,0x08841880,0x3C03089D);if(rc)goto done;
    pause_add(&flow,info,0x08844440,0,1024);
    pause_add(&flow,info,0x08841880,0,104);
    pause_add(&flow,info,0x08841788,0,248);
    for(i=0;i<flow.count && !logger.error && !stopping;++i)pause_node(&flow,info,i,&state);
    if(i<flow.count)++flow.errors;
    if(pause_callers(info,0x08841880,6,&caller_windows,&state))++flow.errors;
    if(pause_callers(info,0x08844440,2,&caller_windows,&state))++flow.errors;
    if(pause_callers(info,0x08841788,0,&caller_windows,&state))++flow.errors;
    rc=flow.errors ? -4 : 0;
 done:
    ra_probe_log_printf(&logger,"GAME_PAUSE_DONE guard_result=%d result=%d nodes=%u node_limit=%u dropped=%u edges=%u total_edges=%u edge_limit=%u errors=%u caller_windows=%u\n",
        guard,rc,flow.count,RA_PAUSE_NODES,flow.dropped,flow.edges,flow.total_edges,RA_PAUSE_EDGES,flow.errors,caller_windows);
    ra_probe_log_printf(&logger,"GAME_TABLE_DONE descriptor_result=%d pause_result=%d windows=%u code_words=%u code_limit=%u binding=UNPROVEN\n",
        guard,rc,state.windows,RA_TABLE_CODE_BUDGET-state.budget,RA_TABLE_CODE_BUDGET);
}
static void audit_game_code(const SceModule* game,const SceKernelModuleInfo* info)
{
    (void)game;
#if RA_CLIENT_HOOK_ENABLED
    {
        ra_table_probe descriptor;
        int table=table_fields(info,"input_descriptor",0x089B4ED0,6,5,0x08842D90,&descriptor);
        int loop=table ? table : pause_signature(info,0x08842CA0,0x0E2105E2);
        ra_probe_log_printf(&logger,"GAME_RUNTIME_GUARD table=%d loop=%d hooks=0 frames=0 binding=UNPROVEN\n",
            table,loop);
    }
#else
    ra_probe_log_printf(&logger,"GAME_TABLE_BEGIN text=0x%08X text_size=%u hooks=0 frames=0 snapshot=EARLY\n",
        (unsigned)info->text_addr,(unsigned)info->text_size);
    audit_table_flow(info);
#endif
}
#endif
