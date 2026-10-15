import sys
import os
import argparse

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from jarvis.llm.manager import LLMManager
from jarvis.llm.mock_backend import MockLLMBackend
from jarvis.llm.llama_backend import LlamaCppBackend
from jarvis.llm.airllm_backend import AirLLMBackend
from jarvis.tools.registry import ToolRegistry
from jarvis.bridge.companion import JarvisCompanion


def main():
    parser = argparse.ArgumentParser(description="JARVIS-OS Host-Side AI Companion")
    parser.add_argument(
        "--backend",
        choices=["mock", "llama", "airllm"],
        default="mock",
        help="LLM inference backend to activate (default: mock)"
    )
    parser.add_argument("--query", type=str, help="Single query to ask JARVIS")
    parser.add_argument("--audit", action="store_true", help="Print tool audit log")
    parser.add_argument("--demo", action="store_true", help="Run automated companion verification demo")

    args = parser.parse_args()

    # Instantiate requested backend
    if args.backend == "llama":
        backend = LlamaCppBackend()
    elif args.backend == "airllm":
        backend = AirLLMBackend()
    else:
        backend = MockLLMBackend()

    llm = LLMManager(backend)
    registry = ToolRegistry()
    companion = JarvisCompanion(llm, registry)

    print("==================================================")
    print("       JARVIS AI COMPANION (HOST SERVICE)         ")
    print("==================================================")
    info = llm.info()
    print(f"Backend:      {info.get('backend')}")
    print(f"Model:        {info.get('model', info.get('model_path', 'N/A'))}")
    print(f"Local:        {info.get('local')}")
    print(f"Available:    {info.get('available', True)}")
    print(f"Tools active: {len(registry.tools)} allowlisted tools")
    print("==================================================")

    if args.demo or not args.query:
        print("\n[Demo Query 1] 'What is the current kernel status?'")
        res1 = companion.query_kernel("What is the current kernel status?")
        print(f"  Tool Executed: {res1.get('tool_called')}")
        print(f"  Telemetry:     {res1.get('tool_result')}")
        print(f"  JARVIS:        {res1.get('llm_response')}")

        print("\n[Demo Query 2] 'Show me the memory usage and free pages.'")
        res2 = companion.query_kernel("Show me the memory usage and free pages.")
        print(f"  Tool Executed: {res2.get('tool_called')}")
        print(f"  Telemetry:     {res2.get('tool_result')}")
        print(f"  JARVIS:        {res2.get('llm_response')}")

        print("\n[Demo Query 3] 'List files stored on the filesystem.'")
        res3 = companion.query_kernel("List files stored on the filesystem.")
        print(f"  Tool Executed: {res3.get('tool_called')}")
        print(f"  Telemetry:     {res3.get('tool_result')}")
        print(f"  JARVIS:        {res3.get('llm_response')}")

        print("\n[Security Check] Testing unauthorized tool call rejection...")
        rej = registry.execute_tool("kernel.dangerous_unrestricted_root_exec", {})
        print(f"  Unauthorized tool result: {rej.get('status')} - {rej.get('error')}")

        print("\n[Audit Trail]:")
        for entry in companion.get_audit_trail():
            print(f"  [{entry['time']}] {entry['tool']} -> {entry['status']} ({entry['ms']})")

    elif args.query:
        res = companion.query_kernel(args.query)
        print(f"\nQuery: {args.query}")
        if res.get("tool_called"):
            print(f"Tool Dispatched: {res.get('tool_called')}")
            print(f"Data:            {res.get('tool_result')}")
        print(f"JARVIS: {res.get('llm_response')}")

        if args.audit:
            print("\nAudit Trail:")
            for entry in companion.get_audit_trail():
                print(f"  [{entry['time']}] {entry['tool']} -> {entry['status']} ({entry['ms']})")


if __name__ == "__main__":
    main()