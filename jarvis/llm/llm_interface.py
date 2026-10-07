from abc import ABC, abstractmethod


class LLMInterface(ABC):

    @abstractmethod
    def generate(self, prompt: str) -> str:
        pass

    @abstractmethod
    def chat(self, messages: list[dict]) -> str:
        pass

    @abstractmethod
    def model_info(self) -> dict:
        pass
