import time
import json
import re
import concurrent.futures
from enum import Enum
from typing import Callable, Dict, Any, List, Optional, Set
from dataclasses import dataclass, field

MAX_ARG_STRING_LENGTH = 1024
MAX_ARGS_PAYLOAD_SIZE = 8192
MAX_OUTPUT_LENGTH = 4096
MAX_AUDIT_LOG_ENTRIES = 128

DANGEROUS_SHELL_PATTERNS = re.compile(r'[;&|`$><*?~\x00]')
SECRET_PATTERNS = re.compile(
    r'(bearer\s+[a-zA-Z0-9_\-\.]+)|(key-[a-zA-Z0-9]+)|(token\s*=\s*[^\s]+)|(password\s*=\s*[^\s]+)|(secret\s*=\s*[^\s]+)',
    re.IGNORECASE
)


def redact_secrets(val: Any) -> Any:
    """Recursively redact sensitive credential patterns from strings, dicts, and lists."""
    if isinstance(val, str):
        return SECRET_PATTERNS.sub('[REDACTED]', val)
    elif isinstance(val, dict):
        return {k: redact_secrets(v) for k, v in val.items()}
    elif isinstance(val, list):
        return [redact_secrets(v) for v in val]
    return val


class PermissionLevel(Enum):
    READ_ONLY = "read_only"
    SAFE_DIAGNOSTIC = "safe_diagnostic"
    PRIVILEGED = "privileged"
    DESTRUCTIVE = "destructive"


@dataclass
class AuditEntry:
    timestamp: float
    tool_name: str
    arguments: Dict[str, Any]
    status: str
    execution_time_ms: float
    error_message: Optional[str] = None


@dataclass
class ToolDefinition:
    name: str
    description: str
    permission: PermissionLevel
    handler: Callable[[Dict[str, Any]], Dict[str, Any]]
    param_schema: Dict[str, type] = field(default_factory=dict)
    required_params: List[str] = field(default_factory=list)
    timeout_sec: float = 5.0

    def get_schema(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "description": self.description,
            "permission": self.permission.value,
            "parameters": {
                k: v.__name__ for k, v in self.param_schema.items()
            },
            "required": self.required_params,
            "timeout_sec": self.timeout_sec
        }


class ToolRegistry:
    """
    Secure tool registry for JARVIS AI Companion.
    Enforces explicit allowlisting, argument schema validation,
    permission confirmation checks for state-changing/destructive actions,
    execution timeouts, and comprehensive timestamped audit logging.
    """

    def __init__(self, allowlist: Optional[List[str]] = None):
        self.tools: Dict[str, ToolDefinition] = {}
        # Default allowlist is strictly restricted to safe system tools
        self.allowlist: Set[str] = set(allowlist) if allowlist is not None else {
            "kernel.query_status",
            "memory.get_stats",
            "scheduler.get_state",
            "storage.list_files",
            "storage.read_file",
            "system.execute_algorithm_demo",
            "storage.format_disk",
            "system.trigger_panic"
        }
        self.audit_log: List[AuditEntry] = []
        self.evicted_audit_count: int = 0
        self._register_default_tools()

    def register_tool(
        self,
        name: str,
        description: str,
        permission: PermissionLevel,
        handler: Callable[[Dict[str, Any]], Dict[str, Any]],
        param_schema: Optional[Dict[str, type]] = None,
        required_params: Optional[List[str]] = None,
        timeout_sec: float = 5.0
    ):
        self.tools[name] = ToolDefinition(
            name=name,
            description=description,
            permission=permission,
            handler=handler,
            param_schema=param_schema or {},
            required_params=required_params or [],
            timeout_sec=timeout_sec
        )

    def is_allowed(self, name: str) -> bool:
        return name in self.allowlist and name in self.tools

    def get_tool_schemas(self) -> List[Dict[str, Any]]:
        return [
            tool.get_schema()
            for name, tool in self.tools.items()
            if name in self.allowlist
        ]

    def execute_tool(
        self,
        name: str,
        arguments: Dict[str, Any],
        approval_granted: bool = False
    ) -> Dict[str, Any]:
        start_time = time.time()

        # 1. Allowlist enforcement
        if name not in self.allowlist:
            error = f"Permission Denied: Tool '{name}' is not in the approved security allowlist."
            self._log_audit(name, arguments, "DENIED", (time.time() - start_time) * 1000, error)
            return {"status": "ERROR", "error": error}

        if name not in self.tools:
            error = f"Not Found: Tool '{name}' is not registered."
            self._log_audit(name, arguments, "NOT_FOUND", (time.time() - start_time) * 1000, error)
            return {"status": "ERROR", "error": error}

        tool = self.tools[name]

        # 2. Permission Level & User Approval Verification
        if tool.permission in (PermissionLevel.PRIVILEGED, PermissionLevel.DESTRUCTIVE):
            if not approval_granted:
                error = (
                    f"Approval Required: Tool '{name}' has permission level '{tool.permission.value}' "
                    f"and requires explicit administrative/user confirmation before execution."
                )
                self._log_audit(name, arguments, "APPROVAL_REQUIRED", (time.time() - start_time) * 1000, error)
                return {
                    "status": "APPROVAL_REQUIRED",
                    "error": error,
                    "permission_level": tool.permission.value
                }

        # 3. Payload size enforcement
        try:
            payload_str = json.dumps(arguments)
            if len(payload_str) > MAX_ARGS_PAYLOAD_SIZE:
                error = f"Validation Error: Total arguments size ({len(payload_str)} bytes) exceeds maximum of {MAX_ARGS_PAYLOAD_SIZE} bytes."
                self._log_audit(name, arguments, "VALIDATION_FAILED", (time.time() - start_time) * 1000, error)
                return {"status": "ERROR", "error": error}
        except Exception:
            pass

        # 4. Required Parameters Check
        for req in tool.required_params:
            if req not in arguments:
                error = f"Validation Error: Missing required parameter '{req}' for tool '{name}'."
                self._log_audit(name, arguments, "VALIDATION_FAILED", (time.time() - start_time) * 1000, error)
                return {"status": "ERROR", "error": error}

        # 5. Parameter Schema Type, Bounds, and Shell Metacharacter Validation
        for param, expected_type in tool.param_schema.items():
            if param in arguments:
                val = arguments[param]
                if not isinstance(val, expected_type):
                    error = (
                        f"Validation Error: Parameter '{param}' expected {expected_type.__name__}, "
                        f"got {type(val).__name__}."
                    )
                    self._log_audit(name, arguments, "VALIDATION_FAILED", (time.time() - start_time) * 1000, error)
                    return {"status": "ERROR", "error": error}

                if isinstance(val, str):
                    if len(val) > MAX_ARG_STRING_LENGTH:
                        error = f"Validation Error: Parameter '{param}' length ({len(val)}) exceeds maximum {MAX_ARG_STRING_LENGTH} chars."
                        self._log_audit(name, arguments, "VALIDATION_FAILED", (time.time() - start_time) * 1000, error)
                        return {"status": "ERROR", "error": error}
                    if DANGEROUS_SHELL_PATTERNS.search(val):
                        error = f"Security Violation: Parameter '{param}' contains prohibited shell metacharacters."
                        self._log_audit(name, arguments, "SECURITY_VIOLATION", (time.time() - start_time) * 1000, error)
                        return {"status": "ERROR", "error": error}

        # 6. Execution within Timeout Protection
        try:
            with concurrent.futures.ThreadPoolExecutor(max_workers=1) as executor:
                future = executor.submit(tool.handler, arguments)
                result = future.result(timeout=tool.timeout_sec)

            # Bounded output truncation
            if isinstance(result, dict):
                res_str = json.dumps(result)
                if len(res_str) > MAX_OUTPUT_LENGTH:
                    result = {
                        "truncated": True,
                        "partial": res_str[:MAX_OUTPUT_LENGTH],
                        "warning": f"[TRUNCATED: output exceeded {MAX_OUTPUT_LENGTH} chars]"
                    }
            elif isinstance(result, str) and len(result) > MAX_OUTPUT_LENGTH:
                result = result[:MAX_OUTPUT_LENGTH] + f"\n[TRUNCATED: output exceeded {MAX_OUTPUT_LENGTH} chars]"

            elapsed = (time.time() - start_time) * 1000
            self._log_audit(name, arguments, "SUCCESS", elapsed)
            return {"status": "SUCCESS", "result": result, "execution_time_ms": elapsed}
        except concurrent.futures.TimeoutError:
            elapsed = (time.time() - start_time) * 1000
            error = f"Timeout Error: Tool '{name}' exceeded execution deadline of {tool.timeout_sec}s."
            self._log_audit(name, arguments, "TIMEOUT", elapsed, error)
            return {"status": "ERROR", "error": error}
        except Exception as e:
            elapsed = (time.time() - start_time) * 1000
            error = f"Execution Exception: {str(e)}"
            self._log_audit(name, arguments, "FAILED", elapsed, error)
            return {"status": "ERROR", "error": error}

    def _log_audit(self, name: str, args: Dict[str, Any], status: str, elapsed: float, err: Optional[str] = None):
        clean_args = redact_secrets(args)
        if len(self.audit_log) >= MAX_AUDIT_LOG_ENTRIES:
            self.audit_log.pop(0)
            self.evicted_audit_count += 1
        self.audit_log.append(AuditEntry(
            timestamp=time.time(),
            tool_name=name,
            arguments=clean_args,
            status=status,
            execution_time_ms=elapsed,
            error_message=err
        ))

    def _register_default_tools(self):
        self.register_tool(
            "kernel.query_status",
            "Retrieve kernel uptime, architecture, and current execution state.",
            PermissionLevel.READ_ONLY,
            lambda args: {
                "telemetry_source": "OFFLINE_SIMULATION",
                "kernel": "x86-64",
                "status": "ONLINE",
                "mode": "Multiboot/LongMode",
                "cpl": 0,
                "pit_hz": 100
            }
        )
        self.register_tool(
            "memory.get_stats",
            "Retrieve physical page allocator and heap usage metrics.",
            PermissionLevel.READ_ONLY,
            lambda args: {
                "telemetry_source": "OFFLINE_SIMULATION",
                "page_size": 4096,
                "total_pages": 64,
                "free_pages": 64,
                "heap_kb": 64,
                "mmu_identity_mapped_mb": 2
            }
        )
        self.register_tool(
            "scheduler.get_state",
            "Retrieve process table count and scheduler configuration.",
            PermissionLevel.READ_ONLY,
            lambda args: {
                "telemetry_source": "OFFLINE_SIMULATION",
                "algorithm": "Round Robin",
                "quantum_ticks": 5,
                "max_processes": 32,
                "preemption_enabled": True
            }
        )
        self.register_tool(
            "storage.list_files",
            "List all files stored on the RAM-disk JARVIS-FS filesystem.",
            PermissionLevel.READ_ONLY,
            lambda args: {
                "telemetry_source": "OFFLINE_SIMULATION",
                "filesystem": "JARVIS-FS",
                "files": ["kernel.cfg", "ai_agent.log"],
                "total_blocks": 128,
                "free_blocks": 120
            }
        )
        self.register_tool(
            "storage.read_file",
            "Read content of a specified file from JARVIS-FS.",
            PermissionLevel.READ_ONLY,
            lambda args: {
                "telemetry_source": "OFFLINE_SIMULATION",
                "filename": args["filename"],
                "content": "JARVIS-OS v1.0\narch=x86-64\nscheduler=RoundRobin\n" if args["filename"] == "kernel.cfg" else ""
            },
            param_schema={"filename": str},
            required_params=["filename"]
        )
        self.register_tool(
            "system.execute_algorithm_demo",
            "Trigger an educational algorithm demonstration (sched, paging, banker, disk).",
            PermissionLevel.SAFE_DIAGNOSTIC,
            lambda args: {
                "telemetry_source": "OFFLINE_SIMULATION",
                "demonstration": args.get("demo_name", "all"),
                "result": "Demo triggered via companion bridge"
            },
            param_schema={"demo_name": str},
            required_params=["demo_name"]
        )
        self.register_tool(
            "storage.format_disk",
            "Format the JARVIS-FS filesystem volume (Destructive operation: clears all files).",
            PermissionLevel.DESTRUCTIVE,
            lambda args: {
                "telemetry_source": "OFFLINE_SIMULATION",
                "action": "FORMAT_VOLUME",
                "status": "FORMATTED",
                "result": "Superblock formatted (0x4A415256). All 120 blocks marked free."
            }
        )
        self.register_tool(
            "system.trigger_panic",
            "Trigger an intentional kernel panic for diagnostics testing (Privileged operation).",
            PermissionLevel.PRIVILEGED,
            lambda args: {
                "telemetry_source": "OFFLINE_SIMULATION",
                "action": "TRIGGER_PANIC",
                "panic_invoked": True,
                "result": "Kernel panic diagnostic triggered."
            }
        )
