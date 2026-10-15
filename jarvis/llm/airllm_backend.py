from typing import List, Dict, Optional
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

    def load(self):
        try:
            from airllm import AutoModel
            self.model = AutoModel.from_pretrained(self.model_name)
        except (ImportError, Exception):
            self.model = None

    def is_available(self) -> bool:
        return self.model is not None

    def generate(self, prompt: str) -> str:
        if not self.is_available():
            return (
                f"[AirLLMBackend: Model not loaded or airllm not installed. "
                f"Falling back to companion mock response for prompt: '{prompt}']"
            )

        input_tokens = self.model.tokenizer(prompt, return_tensors="pt")
        output = self.model.generate(input_tokens["input_ids"], max_new_tokens=128)
        return self.model.tokenizer.decode(output[0])

    def chat(self, messages: List[Dict[str, str]]) -> str:
        last_msg = messages[-1]["content"] if messages else ""
        return self.generate(last_msg)

    def model_info(self) -> dict:
        return {
            "backend": "AirLLM",
            "model": self.model_name,
            "local": True,
            "layer_streaming": True,
            "available": self.is_available()
        }
