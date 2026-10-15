# JARVIS-OS — AI Architecture & Host Companion

## 1. Architectural Philosophy: Kernel vs Host Boundary

Running multi-billion parameter LLMs directly inside a bare-metal kernel is impractical and architectural anti-pattern for an educational operating system. Modern deep-learning libraries depend on complex virtual memory, CUDA/ROCm drivers, multi-threading runtimes, and gigabytes of memory mappings.

Therefore, JARVIS-OS implements a **strictly decoupled AI companion architecture**:

1. **Kernel Space (`kernel/`)**:
   - Zero dependencies on Python, Linux syscalls, or neural network weights.
   - Exposes hardware state, telemetry, and execution commands through the serial console (`COM1` 0x3F8) and debug ports (`0xE9`).
2. **Host Space (`jarvis/`)**:
   - Python companion service running on the developer workstation or host OS.
   - Manages local LLM inference, conversation history, prompt construction, and tool dispatching.
   - Bridges to the bare-metal kernel via structured text or JSON protocols.

---

## 2. Security Boundaries & Tool Registry

To protect the bare-metal kernel from hallucinated or malicious AI commands, the companion enforces a **Zero-Trust Tool Execution Model**:

### 2.1 Explicit Allowlist
The AI companion **cannot** execute arbitrary shell commands. It may only invoke tools that exist in an explicit in-memory allowlist:
- `kernel.query_status`: Queries uptime, architecture, and kernel status (Read-Only).
- `memory.get_stats`: Retrieves page allocator and heap usage metrics (Read-Only).
- `scheduler.get_state`: Inspects active process counts and quantum size (Read-Only).
- `storage.list_files`: Lists directory contents of JARVIS-FS (Sandboxed).
- `system.execute_algorithm_demo`: Triggers deterministic algorithm simulations (Safe).

Any request targeting unapproved commands (such as arbitrary raw pointer writes or unrestricted commands) is rejected immediately with `Permission Denied` without reaching the kernel.

### 2.2 Parameter Validation
All tool definitions specify expected parameter types (`str`, `int`, etc.). If an argument fails type validation, execution is halted before dispatch.

### 2.3 Comprehensive Audit Logging
Every tool invocation records an immutable audit entry:
```python
@dataclass
class AuditEntry:
    timestamp: float
    tool_name: str
    arguments: Dict[str, Any]
    status: str
    execution_time_ms: float
    error_message: Optional[str]
```
The audit log can be inspected via `python3 jarvis/main.py --audit`.

---

## 3. Replaceable LLM Backends

All model backends implement the abstract base class `LLMInterface`:

```python
class LLMInterface(ABC):
    @abstractmethod
    def generate(self, prompt: str) -> str: pass

    @abstractmethod
    def chat(self, messages: list[dict]) -> str: pass

    @abstractmethod
    def model_info(self) -> dict: pass
```

### 3.1 `MockLLMBackend` (Default / Offline)
- Provides deterministic, immediate responses.
- Allows 100% of the AI companion, tool registry, and test suites to run without downloading model weights or requiring GPUs.

### 3.2 `LlamaCppBackend` (Local Quantized GGUF)
- Integrates with `llama-cpp-python` to run quantized 4-bit/8-bit GGUF models locally on CPU or GPU (e.g., Llama-3-8B-Instruct, Mistral-7B).
- Configured via environment variable `JARVIS_LLAMA_MODEL=/path/to/model.gguf`.
- Features automatic offline fallback if weights are not installed.

### 3.3 `AirLLMBackend` (Layer-wise Streaming)
- Employs layer-wise memory streaming to run large 70B models on modest consumer GPUs by loading one transformer layer into VRAM at a time.
- Configured via `JARVIS_AIRLLM_MODEL`.
- Features graceful offline fallback if packages are uninstalled.

---

## 4. Bridge Protocol Schemas

Communication across the Host-Kernel bridge uses structured JSON messages:

### Request Schema
```json
{
  "request_id": 101,
  "action": "tool_call",
  "tool": "kernel.query_status",
  "arguments": {},
  "timestamp": 1729000000.0
}
```

### Response Schema
```json
{
  "request_id": 101,
  "status": "SUCCESS",
  "result": {
    "kernel": "x86-64",
    "status": "ONLINE",
    "mode": "Multiboot/LongMode"
  },
  "error_message": null,
  "execution_time_ms": 0.15
}
```
