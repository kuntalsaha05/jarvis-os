from jarvis.llm.manager import LLMManager


class JarvisAgent:

    def __init__(self, llm: LLMManager):
        self.llm = llm

    def ask(self, prompt: str) -> str:
        return self.llm.generate(prompt)