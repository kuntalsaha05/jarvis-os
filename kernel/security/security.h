#ifndef SECURITY_H
#define SECURITY_H

#include <stdint.h>
#include <stddef.h>

/* Explicit Permission Bitmasks */
#define PERM_NONE           0x00000000U
#define PERM_READ_DIAG      0x00000001U /* System diagnostics, status, dmesg, irq */
#define PERM_FS_READ        0x00000002U /* Read file contents, list directory */
#define PERM_FS_WRITE       0x00000004U /* Write, create, truncate, delete files */
#define PERM_PROC_OP        0x00000008U /* Create/terminate process, scheduler control */
#define PERM_NET_ACCESS     0x00000010U /* Transmit/receive packets, ping, net sockets */
#define PERM_AI_EXEC        0x00000020U /* Dispatch allowed AI companion tools */
#define PERM_ADMIN_OP       0x00000040U /* Shutdown, reboot, panic, format disk */
#define PERM_ALL            0x0000007FU

/* Security Roles */
typedef enum {
    SEC_ROLE_ANONYMOUS = 0, /* No permissions (deny all) */
    SEC_ROLE_GUEST     = 1, /* Read-only diagnostics */
    SEC_ROLE_USER      = 2, /* Read diag, FS read/write, proc ops, network */
    SEC_ROLE_AI_AGENT  = 3, /* Read diag, FS read, AI execution, network (restricted) */
    SEC_ROLE_ADMIN     = 4  /* All permissions (including administrative/destructive) */
} security_role_t;

/* Security Action Identifiers for Audit Logging */
typedef enum {
    SEC_ACT_NONE          = 0,
    SEC_ACT_DIAG_READ     = 1,
    SEC_ACT_FS_READ       = 2,
    SEC_ACT_FS_WRITE      = 3,
    SEC_ACT_FS_DELETE     = 4,
    SEC_ACT_PROC_CREATE   = 5,
    SEC_ACT_PROC_KILL     = 6,
    SEC_ACT_NET_TX        = 7,
    SEC_ACT_NET_RX        = 8,
    SEC_ACT_AI_TOOL       = 9,
    SEC_ACT_SYSCALL       = 10,
    SEC_ACT_IPC_SEND      = 11,
    SEC_ACT_POWER_ACTION  = 12,
    SEC_ACT_MEM_ACCESS    = 13,
    SEC_ACT_AUTH_SWITCH   = 14
} security_action_t;

/* Audit Event Status */
typedef enum {
    SEC_AUDIT_ALLOWED = 0,
    SEC_AUDIT_DENIED  = 1,
    SEC_AUDIT_FAULT   = 2
} security_audit_status_t;

/* Error Codes */
#define SEC_OK                  0
#define SEC_ERR_DENIED         -1
#define SEC_ERR_NULL_PTR       -2
#define SEC_ERR_OVERFLOW       -3
#define SEC_ERR_OUT_OF_BOUNDS  -4
#define SEC_ERR_TRAVERSAL      -5
#define SEC_ERR_INVALID_PARAM  -6
#define SEC_ERR_AUTH_FAILED    -7

/* Security Audit Log Entry */
#define SEC_RESOURCE_NAME_MAX 32
#define SEC_AUDIT_LOG_CAPACITY 32

typedef struct {
    uint64_t timestamp_ticks;
    security_role_t role;
    security_action_t action;
    uint32_t permission_required;
    security_audit_status_t status;
    char resource[SEC_RESOURCE_NAME_MAX];
    int32_t detail_code;
} security_audit_entry_t;

/* Security Telemetry Counters */
typedef struct {
    uint32_t total_events;
    uint32_t allowed_count;
    uint32_t denied_count;
    uint32_t fault_count;
    uint32_t evicted_count;
    security_role_t current_role;
    uint32_t current_permissions;
} security_telemetry_t;

/* Security Subsystem Interface */
void security_init(void);

security_role_t security_get_current_role(void);
const char *security_role_name(security_role_t role);
int security_set_current_role(security_role_t role, const char *auth_token);
uint32_t security_get_role_permissions(security_role_t role);

/* Permission checking & enforcement (deny-by-default) */
int security_check_permission(uint32_t required_perm, security_action_t action, const char *resource, int32_t detail_code);

/* Boundary & Pointer Validation */
int security_validate_buffer(const void *ptr, size_t len, int write_access);
int security_validate_path(const char *path);

/* Audit Logging */
void security_audit_log(security_action_t action, uint32_t required_perm, security_audit_status_t status, const char *resource, int32_t detail_code);
void security_get_telemetry(security_telemetry_t *out_stats);
void security_print_audit_log(void);
void security_print_status(void);

/* Self-test / invariant validation */
int test_security_invariants(void);

#endif /* SECURITY_H */

