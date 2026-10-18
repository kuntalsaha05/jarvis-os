from abc import ABC, abstractmethod
from typing import List, Dict, Any, Optional, Iterator


class LLMInterface(ABC):
    """
    Abstract interface for replaceable local Large Language Model backends.
    Allows JARVIS-OS to swap inference engines (Mock, llama.cpp, AirLLM, etc.)
    without changing companion orchestration or kernel bridge logic.
    """

    @abstractmethod
    def initialize(self) -> bool:
        """Initialize backend resources and load model if configured."""
        pass

    @abstractmethod
    def shutdown(self) -> None:
        """Release backend resources and memory."""
        pass

    @abstractmethod
    def generate(
        self,
        prompt: str,
        max_tokens: int = 256,
        temperature: float = 0.7,
        timeout: float = 30.0
    ) -> str:
        """Generate text completion from a prompt."""
        pass

    def generate_stream(
        self,
        prompt: str,
        max_tokens: int = 256,
        temperature: float = 0.7
    ) -> Iterator[str]:
        """Stream generated text chunks (default generator wraps generate)."""
        yield self.generate(prompt, max_tokens=max_tokens, temperature=temperature)

    @abstractmethod
    def chat(
        self,
        messages: List[Dict[str, str]],
        max_tokens: int = 256,
        temperature: float = 0.7
    ) -> str:
        """Generate chat response from conversation history."""
        pass

    @abstractmethod
    def model_info(self) -> Dict[str, Any]:
        """Return metadata regarding the active backend and model."""
        pass

    @abstractmethod
    def health_check(self) -> Dict[str, Any]:
        """Check status, availability, and readiness of the inference engine."""
        pass
