import json
import os
import urllib.request
import urllib.error
from typing import List, Dict, Any, Optional, Iterator
from .llm_interface import LLMInterface


class OllamaBackend(LLMInterface):
    """
    Real local LLM backend connecting to a local Ollama daemon.
    Communicates over HTTP REST API (default: http://localhost:11434).
    Uses Python standard library without requiring external pip dependencies.
    """

    def __init__(
        self,
        model_name: Optional[str] = None,
        base_url: Optional[str] = None,
        timeout: float = 30.0
    ):
        self.base_url = (base_url or os.environ.get("JARVIS_OLLAMA_URL", "http://localhost:11434")).rstrip("/")
        self.model_name = model_name or os.environ.get("JARVIS_OLLAMA_MODEL", "")
        self.default_timeout = timeout
        self.initialized = False
        self.initialize()

    def initialize(self) -> bool:
        health = self.health_check()
        self.initialized = health.get("status") == "READY"
        return self.initialized

    def shutdown(self) -> None:
        self.initialized = False

    def is_available(self) -> bool:
        health = self.health_check()
        return health.get("status") == "READY"

    def health_check(self) -> Dict[str, Any]:
        """
        Check if Ollama server is reachable and inspect available models.
        """
        tags_url = f"{self.base_url}/api/tags"
        try:
            req = urllib.request.Request(tags_url, headers={"User-Agent": "JARVIS-OS/1.0"})
            with urllib.request.urlopen(req, timeout=3.0) as resp:
                if resp.status == 200:
                    data = json.loads(resp.read().decode("utf-8"))
                    models = [m.get("name") for m in data.get("models", [])]
                    model_found = any(self.model_name in m for m in models) if (self.model_name and models) else False
                    if not model_found and models:
                        self.model_name = models[0]
                    active_model = self.model_name or (models[0] if models else "llama3")
                    return {
                        "backend": "Ollama",
                        "status": "READY",
                        "endpoint": self.base_url,
                        "model": active_model,
                        "available_models": models,
                        "message": f"Connected to Ollama at {self.base_url}. Found {len(models)} local models."
                    }
        except Exception as e:
            return {
                "backend": "Ollama",
                "status": "OFFLINE",
                "endpoint": self.base_url,
                "model": self.model_name,
                "available_models": [],
                "message": f"Cannot connect to Ollama at {self.base_url} ({e}). Run 'ollama serve' to start."
            }

        return {
            "backend": "Ollama",
            "status": "OFFLINE",
            "endpoint": self.base_url,
            "model": self.model_name,
            "message": "Ollama server did not respond."
        }

    def generate(
        self,
        prompt: str,
        max_tokens: int = 256,
        temperature: float = 0.7,
        timeout: float = 30.0
    ) -> str:
        generate_url = f"{self.base_url}/api/generate"
        payload = {
            "model": self.model_name,
            "prompt": prompt,
            "stream": False,
            "options": {
                "temperature": temperature,
                "num_predict": max_tokens
            }
        }

        try:
            req_data = json.dumps(payload).encode("utf-8")
            req = urllib.request.Request(
                generate_url,
                data=req_data,
                headers={"Content-Type": "application/json", "User-Agent": "JARVIS-OS/1.0"}
            )
            with urllib.request.urlopen(req, timeout=timeout or self.default_timeout) as resp:
                if resp.status == 200:
                    data = json.loads(resp.read().decode("utf-8"))
                    return data.get("response", "").strip()
        except urllib.error.URLError as e:
            return (
                f"[Ollama Error: Could not reach Ollama at {self.base_url} ({e.reason}). "
                f"Ensure 'ollama serve' is running and '{self.model_name}' is pulled.]"
            )
        except Exception as e:
            return f"[Ollama Error: {str(e)}]"

        return f"[Ollama: No response received from {self.base_url}]"

    def generate_stream(
        self,
        prompt: str,
        max_tokens: int = 256,
        temperature: float = 0.7
    ) -> Iterator[str]:
        generate_url = f"{self.base_url}/api/generate"
        payload = {
            "model": self.model_name,
            "prompt": prompt,
            "stream": True,
            "options": {
                "temperature": temperature,
                "num_predict": max_tokens
            }
        }

        try:
            req_data = json.dumps(payload).encode("utf-8")
            req = urllib.request.Request(
                generate_url,
                data=req_data,
                headers={"Content-Type": "application/json", "User-Agent": "JARVIS-OS/1.0"}
            )
            with urllib.request.urlopen(req, timeout=self.default_timeout) as resp:
                for line in resp:
                    if line:
                        chunk = json.loads(line.decode("utf-8"))
                        text = chunk.get("response", "")
                        if text:
                            yield text
        except Exception as e:
            yield f"[Ollama Stream Error: {str(e)}]"

    def chat(
        self,
        messages: List[Dict[str, str]],
        max_tokens: int = 256,
        temperature: float = 0.7
    ) -> str:
        chat_url = f"{self.base_url}/api/chat"
        payload = {
            "model": self.model_name,
            "messages": messages,
            "stream": False,
            "options": {
                "temperature": temperature,
                "num_predict": max_tokens
            }
        }

        try:
            req_data = json.dumps(payload).encode("utf-8")
            req = urllib.request.Request(
                chat_url,
                data=req_data,
                headers={"Content-Type": "application/json", "User-Agent": "JARVIS-OS/1.0"}
            )
            with urllib.request.urlopen(req, timeout=self.default_timeout) as resp:
                if resp.status == 200:
                    data = json.loads(resp.read().decode("utf-8"))
                    msg = data.get("message", {})
                    return msg.get("content", "").strip()
        except Exception as e:
            # Fallback to prompt-based generate
            prompt = "\n".join(f"{m.get('role', 'user')}: {m.get('content', '')}" for m in messages)
            prompt += "\nassistant: "
            return self.generate(prompt, max_tokens=max_tokens, temperature=temperature)

        return ""

    def model_info(self) -> Dict[str, Any]:
        health = self.health_check()
        return {
            "backend": "Ollama",
            "model": self.model_name,
            "endpoint": self.base_url,
            "local": True,
            "status": health.get("status"),
            "available": health.get("status") == "READY"
        }

