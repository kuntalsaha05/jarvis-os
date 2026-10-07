from .llm_interface import LLMInterface


class AirLLMBackend(LLMInterface):

    def __init__(self, model_name: str):
        self.model_name = model_name
        self.model = None

    def load(self):
        print(f"Loading local model: {self.model_name}")

    def generate(self, prompt: str) -> str:
        raise NotImplementedError

    def chat(self, messages: list[dict]) -> str:
        raise NotImplementedError

    def model_info(self) -> dict:
        return {
            "backend": "AirLLM",
            "model": self.model_name,
            "local": True
        }

