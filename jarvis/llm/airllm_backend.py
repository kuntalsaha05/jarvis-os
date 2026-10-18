from typing import List, Dict, Any, Optional
import os
from .llm_interface import LLMInterface


class AirLLMBackend(LLMInterface):
    """
    Replaceable local LLM backend powered by AirLLM.
    Enables running large 70B models on limited VRAM via layer-wise streaming.
    Provides graceful offline handling when dependencies or weights are missing.
    """

    def __init__(self, model_name: Optional[str] = None):
        self.model_name = model_name or os.environ.get("JARVIS_AIRLLM_MODEL", "garage-bAInd/Platypus2-70B-instruct")
        self.model = None
        self.initialize()

    def initialize(self) -> bool:
        try:
            from airllm import AutoModel
            self.model = AutoModel.from_pretrained(self.model_name)
            return True
        except (ImportError, Exception):
            self.model = None
            return False

    def shutdown(self) -> None:
        self.model = None

    def is_available(self) -> bool:
        return self.model is not None

    def generate(
        self,
        prompt: str,
        max_tokens: int = 128,
        temperature: float = 0.7,
        timeout: float = 30.0
    ) -> str:
        if not self.is_available():
            return (
                f"[AirLLMBackend: Model not loaded or airllm not installed. "
                f"Falling back to companion mock response for prompt: '{prompt}']"
            )

        try:
            input_tokens = self.model.tokenizer(prompt, return_tensors="pt")
            output = self.model.generate(input_tokens["input_ids"], max_new_tokens=max_tokens)
            return self.model.tokenizer.decode(output[0])
        except Exception as e:
            return f"[AirLLMBackend Error: {str(e)}]"

    def chat(
        self,
        messages: List[Dict[str, str]],
        max_tokens: int = 128,
        temperature: float = 0.7
    ) -> str:
        last_msg = messages[-1]["content"] if messages else ""
        return self.generate(last_msg, max_tokens=max_tokens, temperature=temperature)

    def model_info(self) -> Dict[str, Any]:
        return {
            "backend": "AirLLM",
            "model": self.model_name,
            "local": True,
            "layer_streaming": True,
            "available": self.is_available()
        }

    def health_check(self) -> Dict[str, Any]:
        has_lib = False
        try:
            import airllm  # noqa: F401
            has_lib = True
        except ImportError:
            has_lib = False

        return {
            "backend": "AirLLM",
            "status": "READY" if self.is_available() else "OFFLINE",
            "library_installed": has_lib,
            "model_configured": self.model_name,
            "hardware_prerequisites": "4GB+ GPU VRAM, 32GB+ Host RAM, PyTorch with CUDA",
            "setup_command": "pip install airllm torch && export JARVIS_AIRLLM_MODEL=garage-bAInd/Platypus2-70B-instruct"
        }
