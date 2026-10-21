#ifndef RA_LOG_HISTORY_H
#define RA_LOG_HISTORY_H
#include "kernel_format.h"
/* Bootstrap owns rotation of BOTH logs before loading the user module.
 * No heap, directory scan, secret file or independent user-side counter. */
#define RA_LOG_HISTORY_PREVIOUS 3u
#define RA_LOG_HISTORY_ERROR (-35422)
static int ra_log_missing(int rc)
{ return (uint32_t)rc == UINT32_C(0x80010002); }
static void ra_log_path(char* path, const char* component, unsigned age)
{
    if (age) ra_kernel_snprintf(path, 96, RA_CLIENT_DIR "client-%s.log.%u", component, age);
    else ra_kernel_snprintf(path, 96, RA_CLIENT_DIR "client-%s.log", component);
}
static int ra_log_previous_id(const char* path, uint32_t* highest)
{
    char bytes[32];
    static const char prefix[] = "LOG_SESSION id=";
    uint32_t id = 0;
    unsigned i;
    int fd = sceIoOpen(path, PSP_O_RDONLY, 0), count, close_rc;
    if (fd < 0) return ra_log_missing(fd) ? 0 : fd;
    count = sceIoRead(fd, bytes, sizeof(bytes));
    close_rc = sceIoClose(fd);
    if (count < 0) return count;
    if (close_rc < 0) return close_rc;
    /* Legacy logs (before numbered sessions) start a fresh sequence. */
    if (count < (int)sizeof(prefix)) {
        if (count > 0 && !memcmp(bytes, prefix, (unsigned)count < sizeof(prefix)-1 ? (unsigned)count : sizeof(prefix)-1))
            return RA_LOG_HISTORY_ERROR;
        return 0;
    }
    if (memcmp(bytes, prefix, sizeof(prefix)-1)) return 0;
    i = sizeof(prefix)-1;
    if (bytes[i] < '0' || bytes[i] > '9') return RA_LOG_HISTORY_ERROR;
    for (; i < (unsigned)count && bytes[i] >= '0' && bytes[i] <= '9'; ++i) {
        unsigned digit = (unsigned)(bytes[i]-'0');
        if (id > (UINT32_MAX-digit)/10u) return RA_LOG_HISTORY_ERROR;
        id = id*10u+digit;
    }
    if (i == (unsigned)count || bytes[i] != ' ' || !id) return RA_LOG_HISTORY_ERROR;
    if (id > *highest) *highest = id;
    return 0;
}
static int ra_log_rotate(const char* component)
{
    char source[96], target[96];
    unsigned age;
    int rc;
    ra_log_path(target, component, RA_LOG_HISTORY_PREVIOUS);
    rc = sceIoRemove(target);
    if (rc < 0 && !ra_log_missing(rc)) return rc;
    for (age = RA_LOG_HISTORY_PREVIOUS; age; --age) {
        ra_log_path(source, component, age-1);
        ra_log_path(target, component, age);
        rc = sceIoRename(source, target);
        if (rc < 0 && !ra_log_missing(rc)) return rc;
    }
    return 0;
}
static int ra_log_begin_history(uint32_t* session)
{
    char path[96];
    uint32_t highest = 0;
    unsigned age;
    int rc;
    /* Also recover after interruption between rename and new log creation. */
    for (age = 0; age <= RA_LOG_HISTORY_PREVIOUS; ++age) {
        ra_log_path(path, "bootstrap", age);
        rc = ra_log_previous_id(path, &highest);
        if (rc < 0) return rc;
    }
    if (highest == UINT32_MAX) return RA_LOG_HISTORY_ERROR;
    rc = ra_log_rotate("bootstrap"); if (rc < 0) return rc;
    rc = ra_log_rotate("user"); if (rc < 0) return rc;
    *session = highest+1;
    return 0;
}
#endif
