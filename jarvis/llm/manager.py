from .llm_interface import LLMInterface


class LLMManager:

    def __init__(self, backend: LLMInterface):
        self.backend = backend

    def generate(self, prompt: str) -> str:
        return self.backend.generate(prompt)

    def chat(self, messages: list[dict]) -> str:
        return self.backend.chat(messages)

    def info(self) -> dict:
        return self.backend.model_info()