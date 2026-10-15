from typing import List, Dict, Optional
import os
from .llm_interface import LLMInterface


class LlamaCppBackend(LLMInterface):
    """
    Replaceable local LLM backend powered by llama.cpp / llama-cpp-python.
    Runs quantized models (GGUF format) locally without internet connection.
    Gracefully handles offline or uninstalled environments.
    """

    def __init__(self, model_path: Optional[str] = None, n_ctx: int = 2048):
        self.model_path = model_path or os.environ.get("JARVIS_LLAMA_MODEL", "")
        self.n_ctx = n_ctx
        self.llm = None
        self._init_backend()

    def _init_backend(self):
        if not self.model_path or not os.path.exists(self.model_path):
            # Model file not found; remains in offline/unloaded state
            return

        try:
            from llama_cpp import Llama
            self.llm = Llama(
                model_path=self.model_path,
                n_ctx=self.n_ctx,
                verbose=False
            )
        except ImportError:
            # llama-cpp-python not installed on host
            self.llm = None

    def is_available(self) -> bool:
        return self.llm is not None

    def generate(self, prompt: str) -> str:
        if not self.is_available():
            return (
                f"[LlamaCppBackend: Model not loaded or llama-cpp-python not installed. "
                f"Falling back to companion mock response for prompt: '{prompt}']"
            )

        output = self.llm(
            prompt,
            max_tokens=256,
            stop=["\njarvis>", "User:"],
            echo=False
        )
        return output["choices"][0]["text"].strip()

    def chat(self, messages: List[Dict[str, str]]) -> str:
        if not self.is_available():
            last_msg = messages[-1]["content"] if messages else ""
            return f"[LlamaCppBackend (Offline)]: Echo response for: {last_msg}"

        # Format chat using standard chat template
        prompt = ""
        for m in messages:
            role = m.get("role", "user").capitalize()
            content = m.get("content", "")
            prompt += f"{role}: {content}\n"
        prompt += "Assistant: "

        return self.generate(prompt)

    def model_info(self) -> dict:
        return {
            "backend": "llama.cpp",
            "model_path": self.model_path or "Not configured (Set JARVIS_LLAMA_MODEL)",
            "local": True,
            "quantized": True,
            "available": self.is_available()
        }

