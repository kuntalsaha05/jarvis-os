from typing import List, Dict, Any, Iterator
from .llm_interface import LLMInterface


class MockLLMBackend(LLMInterface):
    """
    Deterministic mock backend for automated testing and CI environments.
    Does not require model weights or internet access.
    """

    def __init__(self):
        self.initialized = False
        self.initialize()

    def initialize(self) -> bool:
        self.initialized = True
        return True

    def shutdown(self) -> None:
        self.initialized = False

    def generate(
        self,
        prompt: str,
        max_tokens: int = 256,
        temperature: float = 0.7,
        timeout: float = 30.0
    ) -> str:
        if not self.initialized:
            raise RuntimeError("MockLLMBackend is shut down or uninitialized.")

        if "simulate_timeout" in prompt:
            raise TimeoutError("Inference request timed out after 30.0s")

        if "kernel status" in prompt.lower():
            return "JARVIS: All kernel subsystems report operational status. CPU Ring 0 execution stable."
        elif "memory" in prompt.lower():
            return "JARVIS: Memory subsystem report shows 64 frames available in the physical page pool."
        elif "file" in prompt.lower() or "storage" in prompt.lower():
            return "JARVIS: JARVIS-FS volume verified with 120 free data blocks on RAM disk."
        elif "hi" in prompt.lower().split() or "hello" in prompt.lower() or "hey" in prompt.lower():
            return "JARVIS: Hello! I am your AI system companion for JARVIS-OS. How can I assist you today?"

        return "JARVIS test response."

    def generate_stream(
        self,
        prompt: str,
        max_tokens: int = 256,
        temperature: float = 0.7
    ) -> Iterator[str]:
        full_text = self.generate(prompt, max_tokens=max_tokens, temperature=temperature)
        words = full_text.split(" ")
        for i, w in enumerate(words):
            yield w + (" " if i < len(words) - 1 else "")

    def chat(
        self,
        messages: List[Dict[str, str]],
        max_tokens: int = 256,
        temperature: float = 0.7
    ) -> str:
        last_msg = messages[-1]["content"] if messages else ""
        return self.generate(last_msg, max_tokens=max_tokens, temperature=temperature)

    def model_info(self) -> Dict[str, Any]:
        return {
            "backend": "Mock",
            "model": "DeterministicMock-v1",
            "local": True,
            "quantized": False,
            "available": self.initialized
        }

    def health_check(self) -> Dict[str, Any]:
        return {
            "status": "HEALTHY" if self.initialized else "UNINITIALIZED",
            "backend": "Mock",
            "ready": self.initialized,
            "latency_ms": 0.1
        }
