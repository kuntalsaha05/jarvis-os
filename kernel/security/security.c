#include "security.h"
#include "../drivers/terminal.h"

#ifdef HOST_TEST
#include <stdio.h>
#include <string.h>
static uint64_t s_host_ticks = 1000;
#define GET_TICKS() (s_host_ticks++)
#else
extern volatile uint64_t timer_ticks;
#define GET_TICKS() (timer_ticks)

static int sec_strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static size_t sec_strlen(const char *s)
{
    size_t len = 0;
    while (s && s[len]) {
        len++;
    }
    return len;
}

static void sec_strncpy(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    if (!dst || !src || n == 0) return;
    for (i = 0; i < n - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static const char *sec_strstr(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return NULL;
    if (!*needle) return haystack;
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
            const char *h = haystack;
            const char *n = needle;
            while (*h && *n && (*h == *n)) {
                h++;
                n++;
            }
            if (!*n) return haystack;
        }
    }
    return NULL;
}
#endif

#ifdef HOST_TEST
#define sec_strcmp strcmp
#define sec_strlen strlen
#define sec_strncpy strncpy
#define sec_strstr strstr
#endif

#define ADMIN_AUTH_TOKEN "jarvis_admin_2026"

static security_role_t s_current_role = SEC_ROLE_ADMIN;
static security_audit_entry_t s_audit_log[SEC_AUDIT_LOG_CAPACITY];
static uint32_t s_audit_head = 0;
static uint32_t s_audit_count = 0;
static security_telemetry_t s_telemetry = {0};

void security_init(void)
{
    s_current_role = SEC_ROLE_ADMIN;
    s_audit_head = 0;
    s_audit_count = 0;
    s_telemetry.total_events = 0;
    s_telemetry.allowed_count = 0;
    s_telemetry.denied_count = 0;
    s_telemetry.fault_count = 0;
    s_telemetry.evicted_count = 0;
    s_telemetry.current_role = s_current_role;
    s_telemetry.current_permissions = security_get_role_permissions(s_current_role);

    security_audit_log(SEC_ACT_AUTH_SWITCH, PERM_ADMIN_OP, SEC_AUDIT_ALLOWED, "security_init", 0);
}

security_role_t security_get_current_role(void)
{
    return s_current_role;
}

const char *security_role_name(security_role_t role)
{
    switch (role) {
        case SEC_ROLE_ANONYMOUS: return "ANONYMOUS";
        case SEC_ROLE_GUEST:     return "GUEST";
        case SEC_ROLE_USER:      return "USER";
        case SEC_ROLE_AI_AGENT:  return "AI_AGENT";
        case SEC_ROLE_ADMIN:     return "ADMIN";
        default:                 return "UNKNOWN";
    }
}

uint32_t security_get_role_permissions(security_role_t role)
{
    switch (role) {
        case SEC_ROLE_ANONYMOUS:
            return PERM_NONE;
        case SEC_ROLE_GUEST:
            return PERM_READ_DIAG;
        case SEC_ROLE_USER:
            return PERM_READ_DIAG | PERM_FS_READ | PERM_FS_WRITE | PERM_PROC_OP | PERM_NET_ACCESS;
        case SEC_ROLE_AI_AGENT:
            return PERM_READ_DIAG | PERM_FS_READ | PERM_AI_EXEC | PERM_NET_ACCESS;
        case SEC_ROLE_ADMIN:
            return PERM_ALL;
        default:
            return PERM_NONE;
    }
}

int security_set_current_role(security_role_t role, const char *auth_token)
{
    if (role > SEC_ROLE_ADMIN) {
        security_audit_log(SEC_ACT_AUTH_SWITCH, PERM_ADMIN_OP, SEC_AUDIT_DENIED, "invalid_role", (int32_t)role);
        return SEC_ERR_INVALID_PARAM;
    }

    /* Elevating to ADMIN requires valid authentication token */
    if (role == SEC_ROLE_ADMIN && s_current_role != SEC_ROLE_ADMIN) {
        if (!auth_token || sec_strcmp(auth_token, ADMIN_AUTH_TOKEN) != 0) {
            security_audit_log(SEC_ACT_AUTH_SWITCH, PERM_ADMIN_OP, SEC_AUDIT_DENIED, "admin_elevation", -1);
            return SEC_ERR_AUTH_FAILED;
        }
    }

    security_role_t prev_role = s_current_role;
    s_current_role = role;
    s_telemetry.current_role = s_current_role;
    s_telemetry.current_permissions = security_get_role_permissions(s_current_role);

    security_audit_log(SEC_ACT_AUTH_SWITCH, PERM_NONE, SEC_AUDIT_ALLOWED, security_role_name(role), (int32_t)prev_role);
    return SEC_OK;
}

int security_check_permission(uint32_t required_perm, security_action_t action, const char *resource, int32_t detail_code)
{
    uint32_t role_perms = security_get_role_permissions(s_current_role);

    /* Deny-by-default: all required permission bits must be satisfied */
    if ((role_perms & required_perm) == required_perm) {
        security_audit_log(action, required_perm, SEC_AUDIT_ALLOWED, resource, detail_code);
        return SEC_OK;
    }

    security_audit_log(action, required_perm, SEC_AUDIT_DENIED, resource, detail_code);
    return SEC_ERR_DENIED;
}

int security_validate_buffer(const void *ptr, size_t len, int write_access)
{
    if (!ptr) {
        return SEC_ERR_NULL_PTR;
    }

    uintptr_t addr = (uintptr_t)ptr;

    /* Integer overflow / pointer wrap-around detection */
    if (addr + len < addr) {
        return SEC_ERR_OVERFLOW;
    }

    /* Physical RAM boundary check (max supported 256 MiB in standard QEMU profile) */
    if (addr + len > 0x10000000ULL) {
        return SEC_ERR_OUT_OF_BOUNDS;
    }

    /* Restrict writing directly into low memory / IVT / BIOS data area (< 1 MiB) */
    if (write_access && addr < 0x100000ULL) {
        return SEC_ERR_DENIED;
    }

    return SEC_OK;
}

int security_validate_path(const char *path)
{
    if (!path) {
        return SEC_ERR_NULL_PTR;
    }
    if (path[0] == '\0') {
        return SEC_ERR_INVALID_PARAM;
    }

    size_t len = sec_strlen(path);
    if (len >= 64) {
        return SEC_ERR_OUT_OF_BOUNDS;
    }

    /* Reject directory traversal patterns */
    if (sec_strstr(path, "..") != NULL) {
        return SEC_ERR_TRAVERSAL;
    }

    /* Reject non-printable or control characters */
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)path[i];
        if (c < 32 || c > 126) {
            return SEC_ERR_INVALID_PARAM;
        }
    }

    return SEC_OK;
}

void security_audit_log(security_action_t action, uint32_t required_perm, security_audit_status_t status, const char *resource, int32_t detail_code)
{
    s_telemetry.total_events++;
    if (status == SEC_AUDIT_ALLOWED) {
        s_telemetry.allowed_count++;
    } else if (status == SEC_AUDIT_DENIED) {
        s_telemetry.denied_count++;
    } else {
        s_telemetry.fault_count++;
    }

    uint32_t idx;
    if (s_audit_count < SEC_AUDIT_LOG_CAPACITY) {
        idx = (s_audit_head + s_audit_count) % SEC_AUDIT_LOG_CAPACITY;
        s_audit_count++;
    } else {
        /* Bounded ring buffer eviction under pressure */
        idx = s_audit_head;
        s_audit_head = (s_audit_head + 1) % SEC_AUDIT_LOG_CAPACITY;
        s_telemetry.evicted_count++;
    }

    security_audit_entry_t *entry = &s_audit_log[idx];
    entry->timestamp_ticks = GET_TICKS();
    entry->role = s_current_role;
    entry->action = action;
    entry->permission_required = required_perm;
    entry->status = status;
    entry->detail_code = detail_code;

    if (resource) {
        sec_strncpy(entry->resource, resource, sizeof(entry->resource));
    } else {
        entry->resource[0] = '\0';
    }
}

void security_get_telemetry(security_telemetry_t *out_stats)
{
    if (out_stats) {
        *out_stats = s_telemetry;
        out_stats->current_role = s_current_role;
        out_stats->current_permissions = security_get_role_permissions(s_current_role);
    }
}

static const char *action_name(security_action_t act)
{
    switch (act) {
        case SEC_ACT_DIAG_READ:    return "DIAG_READ";
        case SEC_ACT_FS_READ:      return "FS_READ";
        case SEC_ACT_FS_WRITE:     return "FS_WRITE";
        case SEC_ACT_FS_DELETE:    return "FS_DELETE";
        case SEC_ACT_PROC_CREATE:  return "PROC_CREATE";
        case SEC_ACT_PROC_KILL:    return "PROC_KILL";
        case SEC_ACT_NET_TX:       return "NET_TX";
        case SEC_ACT_NET_RX:       return "NET_RX";
        case SEC_ACT_AI_TOOL:      return "AI_TOOL";
        case SEC_ACT_SYSCALL:      return "SYSCALL";
        case SEC_ACT_IPC_SEND:     return "IPC_SEND";
        case SEC_ACT_POWER_ACTION: return "POWER_ACTION";
        case SEC_ACT_MEM_ACCESS:   return "MEM_ACCESS";
        case SEC_ACT_AUTH_SWITCH:  return "AUTH_SWITCH";
        default:                   return "NONE";
    }
}

static const char *status_name(security_audit_status_t st)
{
    switch (st) {
        case SEC_AUDIT_ALLOWED: return "ALLOWED";
        case SEC_AUDIT_DENIED:  return "DENIED";
        case SEC_AUDIT_FAULT:   return "FAULT";
        default:                return "UNKNOWN";
    }
}

void security_print_status(void)
{
    terminal_write("\n==================================================\n");
    terminal_write("        JARVIS-OS SECURITY & ACCESS CONTROL       \n");
    terminal_write("==================================================\n");
    terminal_write("Current Role       : ");
    terminal_write(security_role_name(s_current_role));
    terminal_write("\nPermission Bitmask : 0x000000");
    terminal_write_hex((uint8_t)security_get_role_permissions(s_current_role));
    terminal_write("\nPrivilege Model    : Single-Privilege (CPL 0 Long Mode)\n");
    terminal_write("Boundary Protection: Software Bounds Validation\n");
    terminal_write("--------------------------------------------------\n");
    terminal_write("Total Audit Events : ");
    terminal_write_number(s_telemetry.total_events);
    terminal_write("\nAllowed Operations : ");
    terminal_write_number(s_telemetry.allowed_count);
    terminal_write("\nDenied Operations  : ");
    terminal_write_number(s_telemetry.denied_count);
    terminal_write("\nFault / Violations : ");
    terminal_write_number(s_telemetry.fault_count);
    terminal_write("\nEvicted Log Events : ");
    terminal_write_number(s_telemetry.evicted_count);
    terminal_write("\n==================================================\n");
}

void security_print_audit_log(void)
{
    terminal_write("\n==================================================================\n");
    terminal_write("                 [KERN_SEC_AUDIT] SECURITY AUDIT LOG              \n");
    terminal_write("==================================================================\n");
    terminal_write("TICKS    ROLE       ACTION       STATUS   RESOURCE\n");
    terminal_write("------------------------------------------------------------------\n");

    for (uint32_t i = 0; i < s_audit_count; i++) {
        uint32_t idx = (s_audit_head + i) % SEC_AUDIT_LOG_CAPACITY;
        security_audit_entry_t *e = &s_audit_log[idx];

        terminal_write_number((uint32_t)e->timestamp_ticks);
        terminal_write("   ");
        terminal_write(security_role_name(e->role));
        terminal_write("   ");
        terminal_write(action_name(e->action));
        terminal_write("   ");
        terminal_write(status_name(e->status));
        terminal_write("   ");
        terminal_write(e->resource);
        terminal_putchar('\n');
    }

    terminal_write("------------------------------------------------------------------\n");
    terminal_write("Log Entries: ");
    terminal_write_number(s_audit_count);
    terminal_write(" / Capacity: ");
    terminal_write_number(SEC_AUDIT_LOG_CAPACITY);
    terminal_write(" (Evicted: ");
    terminal_write_number(s_telemetry.evicted_count);
    terminal_write(")\n==================================================================\n");
}

int test_security_invariants(void)
{
    security_init();

    /* 1. Invariant: Default role is ADMIN with PERM_ALL */
    if (security_get_current_role() != SEC_ROLE_ADMIN) return -1;
    if (security_get_role_permissions(SEC_ROLE_ADMIN) != PERM_ALL) return -2;

    /* 2. Invariant: Deny-by-default role permissions */
    if (security_get_role_permissions(SEC_ROLE_ANONYMOUS) != PERM_NONE) return -3;
    if (security_get_role_permissions(SEC_ROLE_GUEST) != PERM_READ_DIAG) return -4;

    /* 3. Invariant: Role switching & Auth token enforcement */
    int rc = security_set_current_role(SEC_ROLE_GUEST, NULL);
    if (rc != SEC_OK || security_get_current_role() != SEC_ROLE_GUEST) return -5;

    /* Guest cannot perform FS write or admin action */
    rc = security_check_permission(PERM_FS_WRITE, SEC_ACT_FS_WRITE, "test.txt", 0);
    if (rc != SEC_ERR_DENIED) return -6;

    rc = security_check_permission(PERM_ADMIN_OP, SEC_ACT_POWER_ACTION, "shutdown", 0);
    if (rc != SEC_ERR_DENIED) return -7;

    /* Guest can perform read-only diagnostics */
    rc = security_check_permission(PERM_READ_DIAG, SEC_ACT_DIAG_READ, "dmesg", 0);
    if (rc != SEC_OK) return -8;

    /* Elevation to ADMIN without token must fail */
    rc = security_set_current_role(SEC_ROLE_ADMIN, "wrong_token");
    if (rc != SEC_ERR_AUTH_FAILED) return -9;
    if (security_get_current_role() != SEC_ROLE_GUEST) return -10;

    /* Elevation to ADMIN with valid token succeeds */
    rc = security_set_current_role(SEC_ROLE_ADMIN, ADMIN_AUTH_TOKEN);
    if (rc != SEC_OK || security_get_current_role() != SEC_ROLE_ADMIN) return -11;

    /* Admin can perform all operations */
    rc = security_check_permission(PERM_ADMIN_OP, SEC_ACT_POWER_ACTION, "shutdown", 0);
    if (rc != SEC_OK) return -12;

    /* 4. Invariant: Buffer validation */
    if (security_validate_buffer(NULL, 10, 0) != SEC_ERR_NULL_PTR) return -13;
    /* Wrap-around */
    if (security_validate_buffer((void *)(uintptr_t)0xFFFFFFFFFFFFFFF0ULL, 32, 0) != SEC_ERR_OVERFLOW) return -14;
    /* Exceeds 256 MiB boundary */
    if (security_validate_buffer((void *)(uintptr_t)0x20000000ULL, 100, 0) != SEC_ERR_OUT_OF_BOUNDS) return -15;
    /* Write to IVT / BIOS area (< 1 MiB) */
    if (security_validate_buffer((void *)(uintptr_t)0x00050000ULL, 100, 1) != SEC_ERR_DENIED) return -16;
    /* Valid kernel heap / buffer */
    if (security_validate_buffer((void *)(uintptr_t)0x00200000ULL, 1024, 1) != SEC_OK) return -17;

    /* 5. Invariant: Path validation */
    if (security_validate_path(NULL) != SEC_ERR_NULL_PTR) return -18;
    if (security_validate_path("") != SEC_ERR_INVALID_PARAM) return -19;
    if (security_validate_path("../etc/passwd") != SEC_ERR_TRAVERSAL) return -20;
    if (security_validate_path("dir/../../secret") != SEC_ERR_TRAVERSAL) return -21;
    if (security_validate_path("kernel.cfg") != SEC_OK) return -22;

    /* 6. Invariant: Audit log ring buffer bounded capacity & eviction */
    for (int i = 0; i < 40; i++) {
        security_audit_log(SEC_ACT_DIAG_READ, PERM_READ_DIAG, SEC_AUDIT_ALLOWED, "stress_log", i);
    }
    security_telemetry_t tel;
    security_get_telemetry(&tel);
    if (tel.total_events < 40) return -23;
    if (tel.evicted_count == 0) return -24;
    if (s_audit_count != SEC_AUDIT_LOG_CAPACITY) return -25;

    /* Restore clean admin state */
    security_init();

    return 0;
}

