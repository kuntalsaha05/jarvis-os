from .llm_interface import LLMInterface


class MockLLMBackend(LLMInterface):

    def generate(self, prompt: str) -> str:
        return "JARVIS test response."

    def chat(self, messages: list[dict]) -> str:
        return "JARVIS test response."

    def model_info(self) -> dict:
        return {
            "backend": "Mock",
            "model": "Test",
            "local": True
        }
