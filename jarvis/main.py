from jarvis.agent.agent import JarvisAgent
from jarvis.llm.manager import LLMManager
from jarvis.llm.mock_backend import MockLLMBackend


def main():

    backend = MockLLMBackend()
    llm = LLMManager(backend)
    jarvis = JarvisAgent(llm)

    print("================================")
    print("       J A R V I S")
    print("================================")

    print("Backend:", llm.info()["backend"])

    response = jarvis.ask("Hello JARVIS")

    print("JARVIS:", response)


if __name__ == "__main__":
    main()