import time
from typing import Dict, Any, Optional
from jarvis.llm.manager import LLMManager
from jarvis.tools.registry import ToolRegistry
from jarvis.bridge.protocol import BridgeRequest, BridgeResponse


class JarvisCompanion:
    """
    Host-Side JARVIS Companion Service.
    Orchestrates AI inference, assembles prompt context, validates allowlisted tool calls,
    and bridges host intelligence with the bare-metal kernel environment.
    """

    def __init__(self, llm_manager: LLMManager, tool_registry: Optional[ToolRegistry] = None):
        self.llm = llm_manager
        self.registry = tool_registry or ToolRegistry()
        self.request_counter = 0

    def query_kernel(self, user_prompt: str) -> Dict[str, Any]:
        """
        Process a user prompt: assemble context, decide if an allowlisted tool is needed,
        execute tool securely, and format the final response.
        """
        self.request_counter += 1
        req_id = self.request_counter

        lower = user_prompt.lower()
        tool_to_call = None
        tool_args: Dict[str, Any] = {}

        if "status" in lower or "uptime" in lower or "kernel" in lower:
            tool_to_call = "kernel.query_status"
        elif "memory" in lower or "page" in lower or "heap" in lower:
            tool_to_call = "memory.get_stats"
        elif "process" in lower or "schedule" in lower or "quantum" in lower:
            tool_to_call = "scheduler.get_state"
        elif "file" in lower or "disk" in lower or "directory" in lower or "storage" in lower:
            tool_to_call = "storage.list_files"
        elif "demo" in lower or "banker" in lower or "benchmark" in lower:
            tool_to_call = "system.execute_algorithm_demo"
            tool_args = {"demo_name": "banker" if "banker" in lower else "all"}

        tool_result = None
        if tool_to_call:
            req = BridgeRequest(request_id=req_id, action="tool_call", tool=tool_to_call, arguments=tool_args)
            t_res = self.registry.execute_tool(req.tool, req.arguments)
            if t_res["status"] == "SUCCESS":
                tool_result = t_res["result"]
            else:
                return {
                    "request_id": req_id,
                    "status": "ERROR",
                    "error": t_res.get("error", "Tool execution failed"),
                    "llm_response": "I encountered an error accessing kernel data."
                }

        # Assemble prompt with tool context for LLM
        context_str = f"Kernel telemetry: {tool_result}" if tool_result else "No telemetry requested."
        system_prompt = (
            f"You are JARVIS, an AI assistant monitoring the JARVIS-OS x86-64 educational kernel.\n"
            f"Context: {context_str}\n"
            f"User Question: {user_prompt}\n"
            f"Provide a concise, helpful response:"
        )

        response_text = self.llm.generate(system_prompt)

        return {
            "request_id": req_id,
            "status": "SUCCESS",
            "tool_called": tool_to_call,
            "tool_result": tool_result,
            "llm_response": response_text,
            "backend": self.llm.info()["backend"]
        }

    def get_audit_trail(self):
        return [
            {
                "time": time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(e.timestamp)),
                "tool": e.tool_name,
                "status": e.status,
                "ms": f"{e.execution_time_ms:.2f}ms"
            }
            for e in self.registry.audit_log
        ]

