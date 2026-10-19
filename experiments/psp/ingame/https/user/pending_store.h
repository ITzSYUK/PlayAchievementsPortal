#ifndef RA_PENDING_STORE_H
#define RA_PENDING_STORE_H
/* Only worker does storage. The owner publishes bounded snapshots without I/O.
 * Two complete authenticated files: write the older slot, sync, close, read back.
 * A torn write never replaces the other valid generation. */
static unsigned char pending_key[32],pending_wire[RA_PENDING_WIRE_MAX];
static ra_pending pending_publication,pending_dirty;
static uint32_t pending_gate,pending_ready,pending_seen;
static unsigned pending_enabled,pending_initialized,pending_blocked,pending_slot,pending_written;
static uint64_t pending_retry_at;
static char pending_path[192];
static int pending_mac(void* unused,const void* data,size_t size,unsigned char out[32])
{
    mbedtls_sha256_context sha;unsigned char pad[64],inner[32];unsigned i;int rc;
    (void)unused;
    memset(pad,0x36,64);for(i=0;i<32;++i)pad[i]^=pending_key[i];
    mbedtls_sha256_init(&sha);
    rc=mbedtls_sha256_starts(&sha,0);
    if(!rc)rc=mbedtls_sha256_update(&sha,pad,64);
    if(!rc)rc=mbedtls_sha256_update(&sha,data,size);
    if(!rc)rc=mbedtls_sha256_finish(&sha,inner);
    memset(pad,0x5c,64);for(i=0;i<32;++i)pad[i]^=pending_key[i];
    if(!rc)rc=mbedtls_sha256_starts(&sha,0);
    if(!rc)rc=mbedtls_sha256_update(&sha,pad,64);
    if(!rc)rc=mbedtls_sha256_update(&sha,inner,32);
    if(!rc)rc=mbedtls_sha256_finish(&sha,out);
    mbedtls_sha256_free(&sha);
    mbedtls_platform_zeroize(pad,sizeof(pad));mbedtls_platform_zeroize(inner,sizeof(inner));return rc==0;
}
static void pending_capture(void)
{
    unsigned generation;
    if(!pending_enabled || (generation=ra_session_pending_generation(runtime))==pending_seen || !generation ||
       __sync_lock_test_and_set(&pending_gate,1))return;
    if(ra_session_pending_snapshot(runtime,&pending_publication)) {
        pending_seen=generation;__atomic_store_n(&pending_ready,1,__ATOMIC_RELEASE);
    }
    __sync_lock_release(&pending_gate);
}
static int pending_read(unsigned slot,ra_pending* out,const ra_pending* expected)
{
    SceUID fd;unsigned used=0;int n,rc;unsigned char extra;
    pending_path[strlen(pending_path)-5]=(char)('0'+slot);
    fd=sceIoOpen(pending_path,PSP_O_RDONLY,0);
    if(fd<0)return (uint32_t)fd==UINT32_C(0x80010002)?0:-2;
    while(used<sizeof(pending_wire)) {
        n=sceIoRead(fd,pending_wire+used,sizeof(pending_wire)-used);
        if(n<=0)break;
        if((unsigned)n>sizeof(pending_wire)-used){n=-1;break;}
        used+=(unsigned)n;
    }
    rc=sceIoRead(fd,&extra,1);
    if(sceIoClose(fd)<0 || n<0 || rc<0)return -2;
    if(rc!=0 || !ra_pending_decode(out,pending_wire,used,expected,pending_mac,NULL))return -1;
    return 1;
}
static int pending_load(void)
{
    ra_pending identity,candidate,latest;unsigned char digest[32];unsigned i;int found=0,invalid=0,io_failed=0,rc;
    static const char hex[]="0123456789abcdef";char account[33];
    if(!pending_enabled || !ra_session_pending_snapshot(runtime,&identity))return 1;
    if(mbedtls_sha256((const unsigned char*)identity.account,strlen(identity.account),digest,0))return 0;
    for(i=0;i<16;++i){account[i*2]=hex[digest[i]>>4];account[i*2+1]=hex[digest[i]&15];}account[32]=0;
    snprintf(pending_path,sizeof(pending_path),RA_CLIENT_DIR "pending-%s-%s.0.raq",account,identity.hash);
    for(i=0;i<2;++i) {
        rc=pending_read(i,&candidate,&identity);
        if(rc==-2)io_failed=1;
        if(rc<0)invalid=1;
        if(rc>0 && (!found || candidate.generation>latest.generation)) {
            latest=candidate;pending_slot=i;found=1;
        }
    }
    pending_initialized=1;
    if(io_failed) {
        pending_blocked=1;ui_network_error(12,0,RA_REASON_STORAGE,-35431);
        ra_probe_log_printf(&logger,"PENDING_REJECT reason=STORAGE_READ no_replay=1 files_retained=1\n");
        return 1;
    }
    if(found) {
        if(!ra_session_pending_restore(runtime,&latest,(uint64_t)utc(NULL))) {
            pending_blocked=1;ui_network_error(12,0,RA_REASON_QUEUE,-35430);
            ra_probe_log_printf(&logger,"PENDING_REJECT reason=SET_OR_TIME no_replay=1 files_retained=1\n");
            return 1;
        }
        pending_written=latest.generation;
        ra_probe_log_printf(&logger,"PENDING_RESTORE records=%u generation=%u account_hash=matched image_hash=matched\n",latest.count,latest.generation);
    } else if(invalid) {
        /* Keep unverified files for the owner; never overwrite or replay them. */
        pending_blocked=1;ui_network_error(12,0,RA_REASON_QUEUE,-35430);
        ra_probe_log_printf(&logger,"PENDING_REJECT reason=MAC_FORMAT_OR_ACCOUNT no_replay=1 files_retained=1\n");
    }
    return 1;
}
static void pending_service(void)
{
    ra_pending verified;SceUID fd;size_t size,used=0;int n=0,rc;
    unsigned slot;
    if(!pending_enabled || !pending_initialized || pending_blocked || now_ms(NULL)<pending_retry_at)return;
    if(__atomic_load_n(&pending_ready,__ATOMIC_ACQUIRE) && !__sync_lock_test_and_set(&pending_gate,1)) {
        pending_dirty=pending_publication;__atomic_store_n(&pending_ready,0,__ATOMIC_RELEASE);
        __sync_lock_release(&pending_gate);
    }
    if(!pending_dirty.generation || pending_dirty.generation<=pending_written)return;
    size=ra_pending_encode(&pending_dirty,pending_wire,sizeof(pending_wire),pending_mac,NULL);
    if(!size)goto failed;
    slot=pending_slot^1;pending_path[strlen(pending_path)-5]=(char)('0'+slot);
    fd=sceIoOpen(pending_path,PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC,0600);
    if(fd<0)goto failed;
    while(used<size) {
        n=sceIoWrite(fd,pending_wire+used,size-used);
        if(n<=0 || (size_t)n>size-used)break;
        used+=(size_t)n;
    }
    rc=sceIoClose(fd);
    if(used!=size || n<=0 || rc<0 || sceIoSync("ms0:",0)<0)goto failed;
    if(pending_read(slot,&verified,&pending_dirty)!=1 || verified.generation!=pending_dirty.generation)goto failed;
    pending_written=pending_dirty.generation;pending_slot=slot;
    ra_probe_log_printf(&logger,"PENDING_SAVED records=%u generation=%u slot=%u\n",verified.count,verified.generation,slot);
    return;
failed:
    pending_retry_at=now_ms(NULL)+5000;
    ui_network_error(12,0,RA_REASON_STORAGE,-35431);
    ra_probe_log_printf(&logger,"PENDING_WRITE_FAILED memory_queue_retained=1 retry_ms=5000\n");
}
#endif
