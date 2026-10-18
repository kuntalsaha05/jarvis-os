import time
from typing import Dict, Any, Optional, Callable
from jarvis.llm.manager import LLMManager
from jarvis.tools.registry import ToolRegistry, PermissionLevel


class JarvisAgent:
    """
    Autonomous AI Agent for JARVIS-OS.
    Executes a structured 8-step agentic loop:
      1. Accept user request.
      2. Build context from available system information.
      3. Determine whether a registered tool is required.
      4. Validate the proposed tool and arguments against schemas.
      5. Request approval for privileged or destructive operations.
      6. Execute the permitted tool safely within execution deadlines.
      7. Return the actual result to the model context.
      8. Produce a synthesized final response disclosing telemetry source and limitations.
    """

    def __init__(
        self,
        llm: LLMManager,
        tool_registry: Optional[ToolRegistry] = None,
        approval_callback: Optional[Callable[[str, Dict[str, Any]], bool]] = None
    ):
        self.llm = llm
        self.registry = tool_registry or ToolRegistry()
        self.approval_callback = approval_callback
        self.cycle_count = 0

    def run_cycle(self, user_request: str) -> Dict[str, Any]:
        self.cycle_count += 1
        cycle_id = self.cycle_count

        # Step 1: Accept request
        req = user_request.strip()

        # Step 2: Build initial context
        available_tools = [t["name"] for t in self.registry.get_tool_schemas()]
        context_meta = {
            "agent": "JARVIS-Agent-v1.0",
            "cycle_id": cycle_id,
            "available_tools_count": len(available_tools)
        }

        # Step 3: Determine whether a registered tool is required
        tool_name, tool_args = self._determine_tool(req)

        tool_result = None
        tool_error = None
        approval_status = "NOT_REQUIRED"
        telemetry_source = "OFFLINE_SIMULATION"

        if tool_name:
            # Step 4: Validate proposed tool & arguments
            if not self.registry.is_allowed(tool_name):
                tool_error = f"Security Error: Tool '{tool_name}' is not in the approved allowlist."
            else:
                tool_def = self.registry.tools.get(tool_name)
                is_privileged = tool_def and tool_def.permission in (
                    PermissionLevel.PRIVILEGED, PermissionLevel.DESTRUCTIVE
                )

                # Step 5: Request approval for privileged / destructive actions
                approval_granted = False
                if is_privileged:
                    approval_status = "PENDING"
                    if self.approval_callback:
                        approval_granted = self.approval_callback(tool_name, tool_args)
                        approval_status = "GRANTED" if approval_granted else "DENIED"
                    else:
                        approval_status = "DENIED_NO_CALLBACK"

                    if not approval_granted:
                        tool_error = (
                            f"Permission Denied: Execution of {tool_def.permission.value} "
                            f"tool '{tool_name}' was not approved."
                        )

                # Step 6: Execute permitted tool
                if not tool_error:
                    exec_res = self.registry.execute_tool(
                        tool_name, tool_args, approval_granted=approval_granted
                    )
                    if exec_res.get("status") == "SUCCESS":
                        tool_result = exec_res.get("result", {})
                        if isinstance(tool_result, dict) and "telemetry_source" in tool_result:
                            telemetry_source = tool_result["telemetry_source"]
                    else:
                        tool_error = exec_res.get("error", "Tool execution failed")

        # Step 7: Return actual result to model context
        prompt = self._build_synthesis_prompt(req, tool_name, tool_result, tool_error, telemetry_source)

        # Step 8: Produce final response disclosing limitations and telemetry truthfulness
        model_response = self.llm.generate(prompt)

        return {
            "cycle_id": cycle_id,
            "user_request": user_request,
            "tool_selected": tool_name,
            "tool_arguments": tool_args,
            "approval_status": approval_status,
            "tool_result": tool_result,
            "tool_error": tool_error,
            "telemetry_source": telemetry_source,
            "final_response": model_response,
            "metadata": context_meta
        }

    def ask(self, prompt: str) -> str:
        cycle = self.run_cycle(prompt)
        return cycle["final_response"]

    def _determine_tool(self, prompt: str) -> tuple[Optional[str], Dict[str, Any]]:
        p = prompt.lower()
        if "panic" in p or "crash" in p:
            return "system.trigger_panic", {}
        elif "format" in p or "wipe" in p:
            return "storage.format_disk", {}
        elif "status" in p or "uptime" in p or "kernel" in p:
            return "kernel.query_status", {}
        elif "memory" in p or "page" in p or "heap" in p or "frames" in p:
            return "memory.get_stats", {}
        elif "process" in p or "scheduler" in p or "quantum" in p or "threads" in p:
            return "scheduler.get_state", {}
        elif "list file" in p or "ls" in p or "directory" in p or "storage" in p:
            return "storage.list_files", {}
        elif "cat" in p or "read file" in p:
            return "storage.read_file", {"filename": "kernel.cfg"}
        elif "demo" in p or "banker" in p:
            return "system.execute_algorithm_demo", {"demo_name": "banker" if "banker" in p else "all"}
        return None, {}

    def _build_synthesis_prompt(
        self,
        user_request: str,
        tool_name: Optional[str],
        tool_result: Optional[Any],
        tool_error: Optional[str],
        telemetry_source: str
    ) -> str:
        context_lines = []
        if tool_name:
            context_lines.append(f"Tool Dispatched: {tool_name}")
            context_lines.append(f"Telemetry Source: {telemetry_source}")
            if tool_result:
                context_lines.append(f"Tool Output: {tool_result}")
            if tool_error:
                context_lines.append(f"Tool Error: {tool_error}")
        else:
            context_lines.append("No system tool required for this general query.")

        context_block = "\n".join(context_lines)
        return (
            f"You are JARVIS, the educational operating system AI companion.\n"
            f"--- Context ---\n{context_block}\n"
            f"--- User Request ---\n{user_request}\n"
            f"Provide an accurate, concise answer explicitly noting whether data is from "
            f"LIVE_KERNEL or OFFLINE_SIMULATION:"
        )