"""
FastAPI Server for JARVIS-OS Web GUI.
Provides REST APIs, real-time WebSocket telemetry streaming,
AI Companion integration, and interactive OS algorithm visualizer endpoints.
"""

import os
import sys
import asyncio
from typing import Dict, Any, List, Optional
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.responses import HTMLResponse, FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

# Add project root to sys.path
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
if PROJECT_ROOT not in sys.path:
    sys.path.insert(0, PROJECT_ROOT)

from jarvis.gui.os_engine import (
    KernelState,
    ProcessManager,
    JarvisFS,
    CPUSchedulerSimulator,
    PageReplacementSimulator,
    BankerSimulator,
    DeadlockDetectionSimulator,
    MemoryFitSimulator,
    DiskSchedulerSimulator,
    ConcurrencySimulator,
    VirtualKernelShell
)
from jarvis.llm.manager import LLMManager
from jarvis.llm.mock_backend import MockLLMBackend
from jarvis.llm.llama_backend import LlamaCppBackend
from jarvis.llm.airllm_backend import AirLLMBackend
from jarvis.llm.ollama_backend import OllamaBackend
from jarvis.tools.registry import ToolRegistry
from jarvis.bridge.companion import JarvisCompanion

app = FastAPI(title="JARVIS-OS Modern Web GUI", version="1.0.0")

# Mount static folder
STATIC_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "static")
TEMPLATES_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "templates")

if os.path.exists(STATIC_DIR):
    app.mount("/static", StaticFiles(directory=STATIC_DIR), name="static")

# Shared In-Memory State
kernel_state = KernelState()
proc_manager = ProcessManager()
jarvis_fs = JarvisFS()
concurrency_sim = ConcurrencySimulator(capacity=4)
virtual_shell = VirtualKernelShell(kernel_state, jarvis_fs, proc_manager)

# AI Companion Setup
tool_registry = ToolRegistry()

# Custom tool handlers that read live simulation state!
def _live_kernel_status(args):
    s = kernel_state.get_summary()
    return {
        "telemetry_source": "SIMULATED_KERNEL",
        "kernel": s["kernel"],
        "status": s["status"],
        "mode": s["mode"],
        "ticks": s["ticks"],
        "uptime_sec": s["uptime_seconds"],
        "context_switches": s["context_switches"]
    }

def _live_memory_stats(args):
    s = kernel_state.get_summary()["memory"]
    return {
        "telemetry_source": "SIMULATED_KERNEL",
        "page_size": s["frame_size_bytes"],
        "total_frames": s["total_frames"],
        "free_frames": s["free_frames"],
        "used_frames": s["used_frames"],
        "heap_kb": s["heap_size_kb"],
        "heap_used_kb": s["heap_used_kb"]
    }

def _live_scheduler_state(args):
    s = kernel_state.get_summary()
    procs = proc_manager.list_processes()
    return {
        "telemetry_source": "SIMULATED_KERNEL",
        "algorithm": s["scheduler"],
        "quantum_ticks": s["quantum"],
        "process_count": len(procs),
        "processes": [{"pid": p["pid"], "name": p["name"], "state": p["state"]} for p in procs]
    }

def _live_list_files(args):
    files = jarvis_fs.list_files()
    return {
        "telemetry_source": "SIMULATED_JARVIS_FS",
        "filesystem": "JARVIS-FS",
        "total_files": len(files),
        "files": [f["name"] for f in files]
    }

# Register live overrides into tool registry
from jarvis.tools.registry import PermissionLevel
tool_registry.register_tool("kernel.query_status", "Query live kernel uptime and status", PermissionLevel.READ_ONLY, _live_kernel_status)
tool_registry.register_tool("memory.get_stats", "Query physical frame allocator and heap", PermissionLevel.READ_ONLY, _live_memory_stats)
tool_registry.register_tool("scheduler.get_state", "Query process table and quantum", PermissionLevel.READ_ONLY, _live_scheduler_state)
tool_registry.register_tool("storage.list_files", "List files on RAM disk", PermissionLevel.READ_ONLY, _live_list_files)

llm_manager = LLMManager(MockLLMBackend())
companion = JarvisCompanion(llm_manager, tool_registry)


# ========================================================
# Pydantic Request Models
# ========================================================

class ShellExecRequest(BaseModel):
    command: str

class AIQueryRequest(BaseModel):
    prompt: str

class BackendSwitchRequest(BaseModel):
    backend: str

class ProcessCreateRequest(BaseModel):
    name: str
    priority: int = 1
    is_thread: bool = False

class ProcessActionRequest(BaseModel):
    pid: int
    action: str  # "block", "unblock", "terminate"

class FileWriteRequest(BaseModel):
    name: str
    content: str

class CPUSchedRequest(BaseModel):
    algorithm: str = "fcfs"  # "fcfs", "sjf", "rr"
    tasks: List[Dict[str, Any]]
    quantum: int = 3

class PageReplaceRequest(BaseModel):
    algorithm: str = "fifo"  # "fifo", "lru", "optimal"
    refs: List[int]
    frame_count: int = 3

class BankerSafetyRequest(BaseModel):
    allocation: List[List[int]]
    max_matrix: List[List[int]]
    available: List[int]

class BankerResourceRequest(BaseModel):
    pid: int
    request: List[int]
    allocation: List[List[int]]
    max_matrix: List[List[int]]
    available: List[int]

class DeadlockDetectRequest(BaseModel):
    allocation: List[List[int]]
    request: List[List[int]]
    available: List[int]

class MemFitRequest(BaseModel):
    blocks: List[int]
    requests: List[int]
    strategy: str = "first"  # "first", "best", "worst"

class DiskSchedRequest(BaseModel):
    initial_head: int = 53
    requests: List[int]
    disk_size: int = 200
    algorithm: str = "fcfs"  # "fcfs", "sstf", "scan", "cscan"

class SyncActionRequest(BaseModel):
    action: str  # "produce", "consume", "start_read", "end_read", "start_write", "end_write"


# ========================================================
# Frontend Route
# ========================================================

@app.get("/", response_class=HTMLResponse)
async def serve_index():
    index_file = os.path.join(TEMPLATES_DIR, "index.html")
    if os.path.exists(index_file):
        with open(index_file, "r", encoding="utf-8") as f:
            return HTMLResponse(content=f.read())
    return HTMLResponse("<h1>JARVIS-OS GUI Loading...</h1>", status_code=200)


# ========================================================
# System & Telemetry Endpoints
# ========================================================

@app.get("/api/status")
async def get_system_status():
    return kernel_state.get_summary()

@app.get("/api/memory")
async def get_memory_info():
    return kernel_state.get_summary()["memory"]

@app.get("/api/processes")
async def get_processes():
    return {
        "processes": proc_manager.list_processes(),
        "count": len(proc_manager.list_processes()),
        "quantum": kernel_state.quantum,
        "scheduler": kernel_state.scheduler_algorithm
    }

@app.post("/api/processes/create")
async def create_process(req: ProcessCreateRequest):
    new_p = proc_manager.create_process(req.name, req.priority, req.is_thread)
    return {"status": "SUCCESS", "process": new_p}

@app.post("/api/processes/action")
async def process_action(req: ProcessActionRequest):
    if req.action == "block":
        ok = proc_manager.set_state(req.pid, "BLOCKED")
    elif req.action == "unblock":
        ok = proc_manager.set_state(req.pid, "READY")
    elif req.action == "terminate":
        ok = proc_manager.terminate_process(req.pid)
    else:
        raise HTTPException(status_code=400, detail="Unknown action")
    return {"status": "SUCCESS" if ok else "ERROR", "pid": req.pid, "action": req.action}


# ========================================================
# Storage & JARVIS-FS Endpoints
# ========================================================

@app.get("/api/storage/files")
async def list_storage_files():
    return {
        "files": jarvis_fs.list_files(),
        "disk": jarvis_fs.get_disk_map()
    }

@app.get("/api/storage/file/{filename}")
async def read_storage_file(filename: str):
    content = jarvis_fs.read_file(filename)
    if content is None:
        raise HTTPException(status_code=404, detail="File not found")
    return {"filename": filename, "content": content}

@app.post("/api/storage/file")
async def write_storage_file(req: FileWriteRequest):
    ok = jarvis_fs.write_file(req.name, req.content)
    if not ok:
        raise HTTPException(status_code=400, detail="Failed to write file (disk full or limit reached)")
    return {"status": "SUCCESS", "filename": req.name}

@app.delete("/api/storage/file/{filename}")
async def delete_storage_file(filename: str):
    ok = jarvis_fs.delete_file(filename)
    if not ok:
        raise HTTPException(status_code=404, detail="File not found")
    return {"status": "SUCCESS", "filename": filename}


# ========================================================
# Algorithm Visualizers Endpoints
# ========================================================

@app.post("/api/algorithms/cpu-sched")
async def run_cpu_sched(req: CPUSchedRequest):
    algo = req.algorithm.lower()
    if algo == "fcfs":
        return CPUSchedulerSimulator.simulate_fcfs(req.tasks)
    elif algo == "sjf":
        return CPUSchedulerSimulator.simulate_sjf(req.tasks)
    elif algo == "rr":
        return CPUSchedulerSimulator.simulate_rr(req.tasks, req.quantum)
    else:
        raise HTTPException(status_code=400, detail="Unsupported algorithm. Use 'fcfs', 'sjf', or 'rr'.")

@app.post("/api/algorithms/page-replace")
async def run_page_replace(req: PageReplaceRequest):
    return PageReplacementSimulator.simulate(req.refs, req.frame_count, req.algorithm)

@app.get("/api/algorithms/belady")
async def run_belady_demo():
    return PageReplacementSimulator.belady_anomaly_demo()

@app.post("/api/algorithms/banker/safety")
async def check_banker_safety(req: BankerSafetyRequest):
    return BankerSimulator.check_safety(req.allocation, req.max_matrix, req.available)

@app.post("/api/algorithms/banker/request")
async def request_banker_resources(req: BankerResourceRequest):
    return BankerSimulator.request_resources(
        req.pid, req.request, req.allocation, req.max_matrix, req.available
    )

@app.post("/api/algorithms/deadlock/detect")
async def detect_deadlock(req: DeadlockDetectRequest):
    return DeadlockDetectionSimulator.detect_deadlock(req.allocation, req.request, req.available)

@app.post("/api/algorithms/mem-fit")
async def run_mem_fit(req: MemFitRequest):
    return MemoryFitSimulator.simulate(req.blocks, req.requests, req.strategy)

@app.post("/api/algorithms/disk-sched")
async def run_disk_sched(req: DiskSchedRequest):
    return DiskSchedulerSimulator.simulate(
        req.initial_head, req.requests, req.disk_size, req.algorithm
    )

@app.get("/api/algorithms/sync/state")
async def get_sync_state():
    return concurrency_sim.get_state()

@app.post("/api/algorithms/sync/action")
async def execute_sync_action(req: SyncActionRequest):
    act = req.action.lower()
    if act == "produce":
        return concurrency_sim.produce()
    elif act == "consume":
        return concurrency_sim.consume()
    elif act == "start_read":
        return concurrency_sim.start_read()
    elif act == "end_read":
        return concurrency_sim.end_read()
    elif act == "start_write":
        return concurrency_sim.start_write()
    elif act == "end_write":
        return concurrency_sim.end_write()
    else:
        raise HTTPException(status_code=400, detail="Unknown sync action")


# ========================================================
# AI Companion & Audit Endpoints
# ========================================================

@app.post("/api/companion/query")
async def query_companion(req: AIQueryRequest):
    result = companion.query_kernel(req.prompt)
    result["audit_trail"] = companion.get_audit_trail()[-10:]
    return result

@app.get("/api/companion/audit")
async def get_companion_audit():
    return {
        "audit_trail": companion.get_audit_trail(),
        "allowlist": list(tool_registry.allowlist),
        "backend": companion.llm.info()
    }

@app.post("/api/companion/backend")
async def switch_backend(req: BackendSwitchRequest):
    b = req.backend.lower()
    if b == "llama":
        new_backend = LlamaCppBackend()
    elif b == "airllm":
        new_backend = AirLLMBackend()
    elif b == "ollama":
        new_backend = OllamaBackend()
    else:
        new_backend = MockLLMBackend()

    companion.llm = LLMManager(new_backend)
    return {"status": "SUCCESS", "backend": companion.llm.info()}


# ========================================================
# Interactive Virtual Shell
# ========================================================

@app.post("/api/shell/exec")
async def exec_shell_command(req: ShellExecRequest):
    output = virtual_shell.execute(req.command)
    return {
        "command": req.command,
        "output": output
    }


# ========================================================
# WebSocket Real-Time Telemetry Stream
# ========================================================

@app.websocket("/ws/telemetry")
async def websocket_telemetry(websocket: WebSocket):
    await websocket.accept()
    try:
        while True:
            kernel_state.update_ticks()
            summary = kernel_state.get_summary()
            await websocket.send_json({
                "type": "TELEMETRY_UPDATE",
                "ticks": summary["ticks"],
                "uptime": summary["uptime_seconds"],
                "context_switches": summary["context_switches"],
                "quantum_preemptions": summary["quantum_preemptions"],
                "free_frames": summary["memory"]["free_frames"],
                "heap_used": summary["memory"]["heap_used_kb"]
            })
            await asyncio.sleep(0.5)
    except WebSocketDisconnect:
        pass
    except Exception:
        pass

