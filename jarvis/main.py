import sys
import os
import argparse

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from jarvis.llm.manager import LLMManager
from jarvis.llm.mock_backend import MockLLMBackend
from jarvis.llm.llama_backend import LlamaCppBackend
from jarvis.llm.airllm_backend import AirLLMBackend
from jarvis.tools.registry import ToolRegistry, PermissionLevel
from jarvis.bridge.protocol import PROTOCOL_VERSION
from jarvis.bridge.companion import JarvisCompanion
from jarvis.agent.agent import JarvisAgent


def cli_approval_callback(tool_name: str, arguments: dict) -> bool:
    print(f"\n[SECURITY ALERT] Privileged/Destructive Tool '{tool_name}' requires explicit user confirmation.")
    print(f"  Arguments: {arguments}")
    try:
        ans = input("  Authorize execution? [y/N]: ").strip().lower()
        return ans in ("y", "yes")
    except Exception:
        return False


def explain_concept(agent: JarvisAgent, concept: str):
    prompt = (
        f"Explain the operating system concept '{concept}', how it is modeled in modern OS architectures, "
        f"and how JARVIS-OS implements or demonstrates it."
    )
    print(f"\n--- JARVIS OS Concept Explanation: {concept} ---")
    resp = agent.ask(prompt)
    print(resp)


def run_fs_diagnostics():
    from scripts.jarvis_fs_tool import JarvisFSImage, FS_MAGIC, FS_TOTAL_DATA_BLOCKS
    print("\n==================================================")
    print("      JARVIS-FS STORAGE & VOLUME DIAGNOSTICS      ")
    print("==================================================")
    img = JarvisFSImage.format_new()
    # Populate a sample configuration file to inspect layout
    img.add_file("kernel.cfg", b"HZ=100\nSCHEDULER=RR\nQUANTUM=5\n")
    sb = img.get_superblock()
    files = img.list_files()
    used_blocks = FS_TOTAL_DATA_BLOCKS - sb['free_blocks']
    print(f"Superblock Magic:     {sb['magic_hex']} ({'VALID' if sb['valid'] else 'INVALID'})")
    print(f"Total Disk Blocks:    {sb['total_blocks']} (512 bytes each, 64 KiB capacity)")
    print(f"Data Blocks Total:    {FS_TOTAL_DATA_BLOCKS}")
    print(f"Free Data Blocks:     {sb['free_blocks']}")
    print(f"Used Data Blocks:     {used_blocks}")
    print(f"Max Inodes:           {sb['inode_count']}")
    print(f"Allocated Files:      {len(files)}")
    for f in files:
        print(f"  - {f['name']:<16} size: {f['size']} bytes, blocks: {f['blocks']}")
    print("==================================================")


def print_status(llm: LLMManager, registry: ToolRegistry):
    info = llm.info()
    health = llm.health_check()
    print("==================================================")
    print("       JARVIS AI COMPANION (HOST SERVICE)         ")
    print("==================================================")
    print(f"Protocol Version: {PROTOCOL_VERSION}")
    print(f"Backend:          {info.get('backend')}")
    print(f"Model:            {info.get('model', info.get('model_path', 'N/A'))}")
    print(f"Local:            {info.get('local')}")
    print(f"Available:        {info.get('available')}")
    print(f"Health Status:    {health.get('status')}")
    print(f"Health Message:   {health.get('message')}")
    if health.get("details"):
        for k, v in health["details"].items():
            print(f"  * {k}: {v}")
    print(f"\nTools Registered ({len(registry.tools)} allowlisted):")
    for t in registry.get_tool_schemas():
        print(f"  - {t['name']:<32} [{t['permission']}] : {t['description']}")
    print("==================================================")


def run_interactive(agent: JarvisAgent):
    print("\nStarting JARVIS Interactive Assistant CLI (type 'exit' or 'quit' to quit)")
    print("Try asking questions like: 'What is the kernel status?', 'Show memory usage', 'List files', or 'explain banker'")
    while True:
        try:
            line = input("\njarvis-ai> ").strip()
            if not line:
                continue
            if line.lower() in ("exit", "quit"):
                print("Exiting JARVIS AI companion.")
                break
            result = agent.run_cycle(line)
            if result.get("tool_selected"):
                print(f"[Tool: {result['tool_selected']} | Approval: {result['approval_status']} | Source: {result['telemetry_source']}]")
            print(f"JARVIS: {result['final_response']}")
        except (KeyboardInterrupt, EOFError):
            print("\nExiting.")
            break


def main():
    parser = argparse.ArgumentParser(description="JARVIS-OS Host-Side AI Companion")
    parser.add_argument(
        "--backend",
        choices=["mock", "llama", "airllm"],
        default="mock",
        help="LLM inference backend to activate (default: mock)"
    )
    parser.add_argument("--query", type=str, help="Single query to ask JARVIS")
    parser.add_argument("--interactive", action="store_true", help="Launch interactive chat session")
    parser.add_argument("--explain", type=str, help="Explain an operating system concept")
    parser.add_argument("--fs-diagnostics", action="store_true", help="Run filesystem image inspection diagnostics")
    parser.add_argument("--status", action="store_true", help="Show system, backend health, and tool registry status")
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
    agent = JarvisAgent(llm, registry, approval_callback=cli_approval_callback)

    if args.fs_diagnostics:
        run_fs_diagnostics()
        return

    if args.status:
        print_status(llm, registry)
        return

    if args.explain:
        explain_concept(agent, args.explain)
        return

    if args.interactive:
        run_interactive(agent)
        return

    # Default demo or single query execution
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

    if args.query:
        cycle = agent.run_cycle(args.query)
        print(f"\nQuery: {args.query}")
        if cycle.get("tool_selected"):
            print(f"Tool Dispatched:  {cycle.get('tool_selected')}")
            print(f"Approval Status:  {cycle.get('approval_status')}")
            print(f"Telemetry Source: {cycle.get('telemetry_source')}")
            print(f"Tool Output:      {cycle.get('tool_result')}")
        print(f"JARVIS: {cycle.get('final_response')}")

        if args.audit:
            print("\nAudit Trail:")
            for entry in companion.get_audit_trail():
                print(f"  [{entry['time']}] {entry['tool']} -> {entry['status']} ({entry['ms']})")

    elif args.demo or not args.query:
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


if __name__ == "__main__":
    main()