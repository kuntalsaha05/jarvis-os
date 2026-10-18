from typing import List, Dict, Any, Optional, Iterator
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
        self.initialize()

    def initialize(self) -> bool:
        if not self.model_path or not os.path.exists(self.model_path):
            self.llm = None
            return False

        try:
            from llama_cpp import Llama
            self.llm = Llama(
                model_path=self.model_path,
                n_ctx=self.n_ctx,
                verbose=False
            )
            return True
        except ImportError:
            self.llm = None
            return False

    def shutdown(self) -> None:
        self.llm = None

    def is_available(self) -> bool:
        return self.llm is not None

    def generate(
        self,
        prompt: str,
        max_tokens: int = 256,
        temperature: float = 0.7,
        timeout: float = 30.0
    ) -> str:
        if not self.is_available():
            return (
                f"[LlamaCppBackend: Model not loaded or llama-cpp-python not installed. "
                f"Falling back to companion mock response for prompt: '{prompt}']"
            )

        try:
            output = self.llm(
                prompt,
                max_tokens=max_tokens,
                temperature=temperature,
                stop=["\njarvis>", "User:"],
                echo=False
            )
            return output["choices"][0]["text"].strip()
        except Exception as e:
            return f"[LlamaCppBackend Error: {str(e)}]"

    def generate_stream(
        self,
        prompt: str,
        max_tokens: int = 256,
        temperature: float = 0.7
    ) -> Iterator[str]:
        if not self.is_available():
            yield self.generate(prompt, max_tokens=max_tokens, temperature=temperature)
            return

        try:
            stream = self.llm(
                prompt,
                max_tokens=max_tokens,
                temperature=temperature,
                stop=["\njarvis>", "User:"],
                stream=True
            )
            for chunk in stream:
                text = chunk["choices"][0]["text"]
                if text:
                    yield text
        except Exception as e:
            yield f"[LlamaCppBackend Stream Error: {str(e)}]"

    def chat(
        self,
        messages: List[Dict[str, str]],
        max_tokens: int = 256,
        temperature: float = 0.7
    ) -> str:
        if not self.is_available():
            last_msg = messages[-1]["content"] if messages else ""
            return f"[LlamaCppBackend (Offline)]: Echo response for: {last_msg}"

        prompt = ""
        for m in messages:
            role = m.get("role", "user").capitalize()
            content = m.get("content", "")
            prompt += f"{role}: {content}\n"
        prompt += "Assistant: "

        return self.generate(prompt, max_tokens=max_tokens, temperature=temperature)

    def model_info(self) -> Dict[str, Any]:
        return {
            "backend": "llama.cpp",
            "model_path": self.model_path or "Not configured (Set JARVIS_LLAMA_MODEL)",
            "local": True,
            "quantized": True,
            "available": self.is_available(),
            "context_window": self.n_ctx
        }

    def health_check(self) -> Dict[str, Any]:
        has_lib = False
        try:
            import llama_cpp  # noqa: F401
            has_lib = True
        except ImportError:
            has_lib = False

        has_weights = bool(self.model_path and os.path.exists(self.model_path))

        return {
            "backend": "llama.cpp",
            "status": "READY" if self.is_available() else "OFFLINE",
            "library_installed": has_lib,
            "model_weights_found": has_weights,
            "model_path": self.model_path or None,
            "setup_command": "pip install llama-cpp-python && export JARVIS_LLAMA_MODEL=/path/to/model.gguf"
        }
