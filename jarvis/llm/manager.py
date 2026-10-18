from typing import List, Dict, Any, Iterator
from .llm_interface import LLMInterface


class LLMManager:
    """
    Manager facade routing LLM requests to the configured active backend.
    """

    def __init__(self, backend: LLMInterface):
        self.backend = backend
        self.backend.initialize()

    def generate(
        self,
        prompt: str,
        max_tokens: int = 256,
        temperature: float = 0.7,
        timeout: float = 30.0
    ) -> str:
        return self.backend.generate(prompt, max_tokens=max_tokens, temperature=temperature, timeout=timeout)

    def generate_stream(
        self,
        prompt: str,
        max_tokens: int = 256,
        temperature: float = 0.7
    ) -> Iterator[str]:
        return self.backend.generate_stream(prompt, max_tokens=max_tokens, temperature=temperature)

    def chat(
        self,
        messages: List[Dict[str, str]],
        max_tokens: int = 256,
        temperature: float = 0.7
    ) -> str:
        return self.backend.chat(messages, max_tokens=max_tokens, temperature=temperature)

    def info(self) -> Dict[str, Any]:
        return self.backend.model_info()

    def health(self) -> Dict[str, Any]:
        return self.backend.health_check()

    def health_check(self) -> Dict[str, Any]:
        return self.backend.health_check()

    def shutdown(self) -> None:
        self.backend.shutdown()