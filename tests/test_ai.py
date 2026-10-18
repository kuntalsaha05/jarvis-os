import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from jarvis.llm.manager import LLMManager
from jarvis.llm.mock_backend import MockLLMBackend
from jarvis.llm.llama_backend import LlamaCppBackend
from jarvis.llm.airllm_backend import AirLLMBackend
from jarvis.tools.registry import ToolRegistry, PermissionLevel
from jarvis.bridge.protocol import BridgeRequest, BridgeResponse
from jarvis.bridge.companion import JarvisCompanion


class TestAICompanion(unittest.TestCase):

    def setUp(self):
        self.mock_backend = MockLLMBackend()
        self.llm_manager = LLMManager(self.mock_backend)
        self.registry = ToolRegistry()
        self.companion = JarvisCompanion(self.llm_manager, self.registry)

    def test_mock_backend(self):
        info = self.mock_backend.model_info()
        self.assertEqual(info["backend"], "Mock")
        self.assertTrue(info["local"])

        response = self.mock_backend.generate("Test prompt")
        self.assertIn("JARVIS", response)

    def test_llama_backend_offline_fallback(self):
        # Without model weights configured, should gracefully report offline
        backend = LlamaCppBackend(model_path="/non/existent/path.gguf")
        info = backend.model_info()
        self.assertEqual(info["backend"], "llama.cpp")
        self.assertFalse(info["available"])

        res = backend.generate("Hello")
        self.assertIn("LlamaCppBackend", res)

    def test_airllm_backend_offline_fallback(self):
        backend = AirLLMBackend(model_name="dummy-70b")
        info = backend.model_info()
        self.assertEqual(info["backend"], "AirLLM")
        self.assertFalse(info["available"])

        res = backend.generate("Hello")
        self.assertIn("AirLLMBackend", res)

    def test_tool_registry_allowlist(self):
        # 1. Allowed tool should succeed
        res = self.registry.execute_tool("kernel.query_status", {})
        self.assertEqual(res["status"], "SUCCESS")
        self.assertEqual(res["result"]["status"], "ONLINE")

        # 2. Unapproved tool must be rejected
        res_denied = self.registry.execute_tool("kernel.dangerous_unapproved_command", {})
        self.assertEqual(res_denied["status"], "ERROR")
        self.assertIn("Permission Denied", res_denied["error"])

    def test_tool_registry_input_validation(self):
        # system.execute_algorithm_demo expects demo_name as str
        valid_res = self.registry.execute_tool("system.execute_algorithm_demo", {"demo_name": "banker"})
        self.assertEqual(valid_res["status"], "SUCCESS")

        # Passing invalid type (int instead of str)
        invalid_res = self.registry.execute_tool("system.execute_algorithm_demo", {"demo_name": 12345})
        self.assertEqual(invalid_res["status"], "ERROR")
        self.assertIn("Validation Error", invalid_res["error"])

    def test_audit_logging(self):
        initial_len = len(self.registry.audit_log)
        self.registry.execute_tool("memory.get_stats", {})
        self.assertEqual(len(self.registry.audit_log), initial_len + 1)

        last_entry = self.registry.audit_log[-1]
        self.assertEqual(last_entry.tool_name, "memory.get_stats")
        self.assertEqual(last_entry.status, "SUCCESS")
        self.assertGreater(last_entry.timestamp, 0)

    def test_companion_query_orchestration(self):
        # Querying kernel status should automatically dispatch kernel.query_status
        out = self.companion.query_kernel("What is the current kernel status?")
        self.assertEqual(out["status"], "SUCCESS")
        self.assertEqual(out["tool_called"], "kernel.query_status")
        self.assertEqual(out["tool_result"]["status"], "ONLINE")
        self.assertIn("JARVIS", out["llm_response"])

    def test_bridge_protocol_serialization(self):
        req = BridgeRequest(request_id=42, action="tool_call", tool="memory.get_stats", arguments={"key": "val"})
        json_str = req.to_json()
        req2 = BridgeRequest.from_json(json_str)
        self.assertEqual(req2.request_id, 42)
        self.assertEqual(req2.tool, "memory.get_stats")
        self.assertEqual(req2.arguments["key"], "val")

        resp = BridgeResponse(request_id=42, status="SUCCESS", result={"uptime": 100})
        resp_json = resp.to_json()
        resp2 = BridgeResponse.from_json(resp_json)
        self.assertEqual(resp2.request_id, 42)
        self.assertEqual(resp2.result["uptime"], 100)

    def test_telemetry_source_truthfulness(self):
        # Tools in offline simulation mode must explicitly state OFFLINE_SIMULATION
        status = self.registry.execute_tool("kernel.query_status", {})
        self.assertEqual(status["result"]["telemetry_source"], "OFFLINE_SIMULATION")

        mem = self.registry.execute_tool("memory.get_stats", {})
        self.assertEqual(mem["result"]["telemetry_source"], "OFFLINE_SIMULATION")

        sched = self.registry.execute_tool("scheduler.get_state", {})
        self.assertEqual(sched["result"]["telemetry_source"], "OFFLINE_SIMULATION")

        storage = self.registry.execute_tool("storage.list_files", {})
        self.assertEqual(storage["result"]["telemetry_source"], "OFFLINE_SIMULATION")

    def test_malformed_bridge_requests(self):
        # Invalid JSON fallback
        try:
            BridgeRequest.from_json("invalid-json-string")
            self.fail("Should raise ValueError or json.JSONDecodeError")
        except Exception:
            pass

    def test_jarvis_fs_image_tool(self):
        from scripts.jarvis_fs_tool import JarvisFSImage, FS_MAGIC
        img = JarvisFSImage.format_new()
        sb = img.get_superblock()
        self.assertEqual(sb["magic"], FS_MAGIC)
        self.assertTrue(sb["valid"])
        self.assertEqual(sb["free_blocks"], 120)

        test_data = b"Testing JARVIS-FS python tool integration"
        img.add_file("host_test.txt", test_data)
        files = img.list_files()
        self.assertEqual(len(files), 1)
        self.assertEqual(files[0]["name"], "host_test.txt")
        self.assertEqual(files[0]["size"], len(test_data))

        readback = img.read_file("host_test.txt")
        self.assertEqual(readback, test_data)

    def test_agent_8_step_cycle(self):
        from jarvis.agent.agent import JarvisAgent
        agent = JarvisAgent(self.llm_manager, self.registry)
        res = agent.run_cycle("What is the current kernel status?")
        self.assertEqual(res["cycle_id"], 1)
        self.assertEqual(res["tool_selected"], "kernel.query_status")
        self.assertEqual(res["approval_status"], "NOT_REQUIRED")
        self.assertEqual(res["telemetry_source"], "OFFLINE_SIMULATION")
        self.assertIsNotNone(res["tool_result"])
        self.assertEqual(res["tool_result"]["status"], "ONLINE")
        self.assertIn("JARVIS", res["final_response"])

    def test_agent_privileged_approval_denied_by_default(self):
        from jarvis.agent.agent import JarvisAgent
        # Without approval callback, privileged actions must be blocked
        agent = JarvisAgent(self.llm_manager, self.registry)
        res = agent.run_cycle("trigger a kernel panic now")
        self.assertEqual(res["tool_selected"], "system.trigger_panic")
        self.assertEqual(res["approval_status"], "DENIED_NO_CALLBACK")
        self.assertIn("Permission Denied", res["tool_error"])
        self.assertIsNone(res["tool_result"])

    def test_agent_privileged_approval_callback_flow(self):
        from jarvis.agent.agent import JarvisAgent

        # 1. Callback returns False -> DENIED
        agent_denied = JarvisAgent(
            self.llm_manager,
            self.registry,
            approval_callback=lambda tool, args: False
        )
        res_denied = agent_denied.run_cycle("trigger a kernel panic")
        self.assertEqual(res_denied["approval_status"], "DENIED")
        self.assertIn("Permission Denied", res_denied["tool_error"])

        # 2. Callback returns True -> GRANTED
        agent_granted = JarvisAgent(
            self.llm_manager,
            self.registry,
            approval_callback=lambda tool, args: True
        )
        res_granted = agent_granted.run_cycle("trigger a kernel panic")
        self.assertEqual(res_granted["approval_status"], "GRANTED")
        self.assertIsNone(res_granted["tool_error"])
        self.assertTrue(res_granted["tool_result"]["panic_invoked"])

    def test_agent_destructive_format_disk(self):
        from jarvis.agent.agent import JarvisAgent
        agent_granted = JarvisAgent(
            self.llm_manager,
            self.registry,
            approval_callback=lambda tool, args: True
        )
        res = agent_granted.run_cycle("format disk volume")
        self.assertEqual(res["tool_selected"], "storage.format_disk")
        self.assertEqual(res["approval_status"], "GRANTED")
        self.assertEqual(res["tool_result"]["status"], "FORMATTED")

    def test_backend_health_checks(self):
        mock_health = self.mock_backend.health_check()
        self.assertEqual(mock_health["status"], "HEALTHY")

        llama = LlamaCppBackend(model_path="/non/existent/model.gguf")
        llama_health = llama.health_check()
        self.assertIn(llama_health["status"], ["OFFLINE", "UNAVAILABLE"])

        airllm = AirLLMBackend(model_name="dummy-70b")
        airllm_health = airllm.health_check()
        self.assertIn(airllm_health["status"], ["OFFLINE", "UNAVAILABLE"])

    def test_mock_streaming_generation(self):
        stream = self.mock_backend.generate_stream("Stream test")
        tokens = list(stream)
        self.assertGreater(len(tokens), 0)
        joined = "".join(tokens)
        self.assertIn("JARVIS", joined)

    def test_serial_transport_framing(self):
        from jarvis.bridge.serial_transport import SerialBridgeTransport
        transport = SerialBridgeTransport()
        raw_msg = transport.format_message({"hello": "world"})
        self.assertTrue(raw_msg.endswith(b"\n"))

        # Test line parsing
        parsed = transport.parse_line(raw_msg.decode("utf-8"))
        self.assertEqual(parsed, {"hello": "world"})

        # Test request dispatch
        req = BridgeRequest(request_id=99, action="tool_call", tool="memory.get_stats", arguments={})
        resp = transport.dispatch_request(req, self.registry)
        self.assertEqual(resp.request_id, 99)
        self.assertEqual(resp.status, "SUCCESS")


if __name__ == "__main__":
    unittest.main()

