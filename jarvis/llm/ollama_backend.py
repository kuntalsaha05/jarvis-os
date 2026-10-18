"""
Ollama Local LLM Backend for J.A.R.V.I.S.
Connects to local Ollama instance (http://localhost:11434) to query models like llama3, mistral, phi3, etc.
Gracefully falls back to the smart assistant engine if Ollama is not running.
"""

import json
import urllib.request
import urllib.error
from typing import Dict, Any, Optional
from .llm_interface import LLMInterface
from .mock_backend import MockLLMBackend


class OllamaBackend(LLMInterface):
    """
    Ollama integration for local open-source LLM inference.
    """

    def __init__(self, host: str = "http://127.0.0.1:11434", model: str = "llama3"):
        self.host = host.rstrip("/")
        self.model = model
        self.fallback = MockLLMBackend()

    def is_available(self) -> bool:
        try:
            req = urllib.request.Request(f"{self.host}/api/tags", method="GET")
            with urllib.request.urlopen(req, timeout=1.5) as res:
                return res.status == 200
        except Exception:
            return False

    def model_info(self) -> dict:
        online = self.is_available()
        return {
            "backend": "Ollama",
            "model": self.model,
            "host": self.host,
            "local": True,
            "available": online
        }

    def generate(self, prompt: str) -> str:
        if not self.is_available():
            # Graceful intelligent fallback
            return self.fallback.generate(prompt)

        try:
            payload = json.dumps({
                "model": self.model,
                "prompt": prompt,
                "stream": False
            }).encode("utf-8")

            req = urllib.request.Request(
                f"{self.host}/api/generate",
                data=payload,
                headers={"Content-Type": "application/json"},
                method="POST"
            )

            with urllib.request.urlopen(req, timeout=15) as res:
                data = json.loads(res.read().decode("utf-8"))
                return f"JARVIS: {data.get('response', '').strip()}"
        except Exception as e:
            return self.fallback.generate(prompt)

    def chat(self, messages: list[dict]) -> str:
        last_msg = messages[-1]["content"] if messages else ""
        return self.generate(last_msg)

