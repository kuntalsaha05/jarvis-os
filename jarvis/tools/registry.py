import time
from enum import Enum
from typing import Callable, Dict, Any, List, Optional
from dataclasses import dataclass, field


class PermissionLevel(Enum):
    READ_ONLY = "read_only"
    PRIVILEGED = "privileged"
    RESTRICTED = "restricted"


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


class ToolRegistry:
    """
    Secure tool registry for JARVIS AI Companion.
    Enforces explicit allowlisting, argument schema validation,
    and comprehensive audit logging.
    """

    def __init__(self, allowlist: Optional[List[str]] = None):
        self.tools: Dict[str, ToolDefinition] = {}
        # Default allowlist is restricted to safe, read-only system tools
        self.allowlist: set[str] = set(allowlist) if allowlist is not None else {
            "kernel.query_status",
            "memory.get_stats",
            "scheduler.get_state",
            "storage.list_files",
            "system.execute_algorithm_demo"
        }
        self.audit_log: List[AuditEntry] = []
        self._register_default_tools()

    def register_tool(
        self,
        name: str,
        description: str,
        permission: PermissionLevel,
        handler: Callable[[Dict[str, Any]], Dict[str, Any]],
        param_schema: Optional[Dict[str, type]] = None
    ):
        self.tools[name] = ToolDefinition(
            name=name,
            description=description,
            permission=permission,
            handler=handler,
            param_schema=param_schema or {}
        )

    def is_allowed(self, name: str) -> bool:
        return name in self.allowlist and name in self.tools

    def execute_tool(self, name: str, arguments: Dict[str, Any]) -> Dict[str, Any]:
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

        # 2. Input validation
        for param, expected_type in tool.param_schema.items():
            if param in arguments:
                val = arguments[param]
                if not isinstance(val, expected_type):
                    error = f"Validation Error: Parameter '{param}' expected {expected_type.__name__}, got {type(val).__name__}."
                    self._log_audit(name, arguments, "VALIDATION_FAILED", (time.time() - start_time) * 1000, error)
                    return {"status": "ERROR", "error": error}

        # 3. Execution with safety boundary
        try:
            result = tool.handler(arguments)
            elapsed = (time.time() - start_time) * 1000
            self._log_audit(name, arguments, "SUCCESS", elapsed)
            return {"status": "SUCCESS", "result": result, "execution_time_ms": elapsed}
        except Exception as e:
            elapsed = (time.time() - start_time) * 1000
            error = f"Execution Exception: {str(e)}"
            self._log_audit(name, arguments, "FAILED", elapsed, error)
            return {"status": "ERROR", "error": error}

    def _log_audit(self, name: str, args: Dict[str, Any], status: str, elapsed: float, err: Optional[str] = None):
        self.audit_log.append(AuditEntry(
            timestamp=time.time(),
            tool_name=name,
            arguments=args,
            status=status,
            execution_time_ms=elapsed,
            error_message=err
        ))

    def _register_default_tools(self):
        self.register_tool(
            "kernel.query_status",
            "Retrieve kernel uptime, architecture, and current state.",
            PermissionLevel.READ_ONLY,
            lambda args: {"kernel": "x86-64", "status": "ONLINE", "mode": "Multiboot/LongMode"}
        )
        self.register_tool(
            "memory.get_stats",
            "Retrieve physical page allocator and heap usage metrics.",
            PermissionLevel.READ_ONLY,
            lambda args: {"page_size": 4096, "total_pages": 64, "free_pages": 64, "heap_kb": 64}
        )
        self.register_tool(
            "scheduler.get_state",
            "Retrieve process table count and scheduler configuration.",
            PermissionLevel.READ_ONLY,
            lambda args: {"algorithm": "Round Robin", "quantum_ticks": 5, "max_processes": 32}
        )
        self.register_tool(
            "storage.list_files",
            "List all files stored on the RAM-disk JARVIS-FS filesystem.",
            PermissionLevel.READ_ONLY,
            lambda args: {"filesystem": "JARVIS-FS", "files": ["kernel.cfg", "ai_agent.log"]}
        )
        self.register_tool(
            "system.execute_algorithm_demo",
            "Trigger an educational algorithm demonstration.",
            PermissionLevel.READ_ONLY,
            lambda args: {"demonstration": args.get("demo_name", "all"), "result": "Demo triggered via companion bridge"},
            param_schema={"demo_name": str}
        )

