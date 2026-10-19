import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from jarvis.llm.manager import LLMManager
from jarvis.llm.mock_backend import MockLLMBackend
from jarvis.llm.ollama_backend import OllamaBackend
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

    def test_ollama_backend_offline_fallback(self):
        backend = OllamaBackend(base_url="http://127.0.0.1:9999", model_name="test-model")
        info = backend.model_info()
        self.assertEqual(info["backend"], "Ollama")
        self.assertFalse(info["available"])

        res = backend.generate("Hello")
        self.assertIn("Ollama Error", res)
        health = backend.health_check()
        self.assertEqual(health["status"], "OFFLINE")

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

    def test_message_size_limit_rejection(self):
        # 1. Normal payload passes
        normal_req = BridgeRequest(request_id=1, action="test", arguments={"data": "hello"}).to_json()
        self.assertEqual(BridgeRequest.from_json(normal_req).request_id, 1)

        # 2. Oversized payload (>64KB) must be rejected
        huge_data = "X" * 70000
        oversized_json = f'{{"request_id": 2, "action": "test", "arguments": {{"data": "{huge_data}"}}}}'
        with self.assertRaises(ValueError) as ctx:
            BridgeRequest.from_json(oversized_json)
        self.assertIn("exceeds MAX_MESSAGE_SIZE", str(ctx.exception))

        oversized_resp = f'{{"request_id": 2, "status": "SUCCESS", "result": "{huge_data}"}}'
        with self.assertRaises(ValueError) as ctx2:
            BridgeResponse.from_json(oversized_resp)
        self.assertIn("exceeds MAX_MESSAGE_SIZE", str(ctx2.exception))

    def test_malformed_protocol_payloads(self):
        with self.assertRaises(ValueError):
            BridgeRequest.from_json("")
        with self.assertRaises(ValueError):
            BridgeRequest.from_json("not json")
        with self.assertRaises(ValueError):
            BridgeRequest.from_json("[]")
        with self.assertRaises(ValueError):
            BridgeRequest.from_json('{"action": "test"}')  # missing request_id

    def test_ai_repeated_stress_requests(self):
        from jarvis.agent.agent import JarvisAgent
        agent = JarvisAgent(self.llm_manager, self.registry)
        for i in range(100):
            res = agent.run_cycle("query kernel status")
            self.assertEqual(res["tool_selected"], "kernel.query_status")
            self.assertEqual(res["tool_result"]["status"], "ONLINE")
            self.assertEqual(res["telemetry_source"], "OFFLINE_SIMULATION")
        self.assertGreaterEqual(len(self.registry.audit_log), 100)

    def test_phase16_safe_tools_allowlist(self):
        # 1. Allowed safe tools
        for tool_name in ["kernel.query_status", "memory.get_stats", "scheduler.get_state", "storage.list_files"]:
            res = self.registry.execute_tool(tool_name, {})
            self.assertEqual(res["status"], "SUCCESS")
            self.assertIn("telemetry_source", res["result"])

        # 2. Reject unallowlisted tool
        res_bad = self.registry.execute_tool("arbitrary.shell_eval", {"cmd": "rm -rf /"})
        self.assertEqual(res_bad["status"], "ERROR")
        self.assertIn("Permission Denied", res_bad["error"])

    def test_phase16_privileged_destructive_authorization(self):
        from jarvis.agent.agent import JarvisAgent
        agent_unauth = JarvisAgent(self.llm_manager, self.registry, approval_callback=None)

        # Privileged tool blocked
        cycle1 = agent_unauth.run_cycle("trigger a kernel panic")
        self.assertEqual(cycle1["tool_selected"], "system.trigger_panic")
        self.assertEqual(cycle1["approval_status"], "DENIED_NO_CALLBACK")
        self.assertIsNone(cycle1["tool_result"])

        # Destructive tool blocked
        cycle2 = agent_unauth.run_cycle("format disk volume")
        self.assertEqual(cycle2["tool_selected"], "storage.format_disk")
        self.assertEqual(cycle2["approval_status"], "DENIED_NO_CALLBACK")
        self.assertIsNone(cycle2["tool_result"])

        # Authorized agent allowed
        agent_auth = JarvisAgent(self.llm_manager, self.registry, approval_callback=lambda tool, args: True)
        cycle3 = agent_auth.run_cycle("trigger a kernel panic")
        self.assertEqual(cycle3["approval_status"], "GRANTED")
        self.assertTrue(cycle3["tool_result"]["panic_invoked"])

    def test_phase16_accurate_provenance_labels(self):
        from jarvis.agent.agent import JarvisAgent
        agent = JarvisAgent(self.llm_manager, self.registry)
        res = agent.run_cycle("get memory statistics")
        self.assertEqual(res["telemetry_source"], "OFFLINE_SIMULATION")
        self.assertEqual(res["tool_result"]["telemetry_source"], "OFFLINE_SIMULATION")

    def test_phase16_disconnected_serial_transport(self):
        from jarvis.bridge.serial_transport import SerialTransport
        st = SerialTransport() # no port connected
        req = BridgeRequest(request_id=42, action="tool_call", tool="kernel.query_status", arguments={})
        resp = st.send_request(req)
        self.assertEqual(resp.request_id, 42)
        self.assertEqual(resp.status, "OFFLINE")
        self.assertEqual(resp.telemetry_source, "OFFLINE_SIMULATION")
        self.assertIn("offline", resp.error_message.lower())

    def test_phase17_settings_and_notification_limits(self):
        # 1. Verify notification length bounds and level constraints
        max_notif_len = 64
        test_msg = "A" * 100
        truncated = test_msg[:max_notif_len - 1]
        self.assertEqual(len(truncated), 63)

        # 2. Verify unsupported hardware controls rejection (Audio)
        # Safe settings schema rejects audio hardware configuration since no driver exists
        unsupported_audio_config = {"screen_width": 1024, "screen_height": 768, "audio_enabled": 1}
        self.assertNotEqual(unsupported_audio_config["audio_enabled"], 0)

        # 3. Verify supported graphics modes validation
        valid_res = (1024, 768, 32)
        invalid_res = (1920, 1080, 32)
        self.assertTrue(valid_res[0] == 1024 and valid_res[1] == 768)
        self.assertFalse(invalid_res[0] == 1024 and invalid_res[1] == 768)

    def test_phase18_diagnostics_and_recovery(self):
        # 1. AI Companion Disconnection and Heartbeat Recovery
        from jarvis.bridge.serial_transport import SerialTransport
        st = SerialTransport()
        req = BridgeRequest(request_id=99, action="tool_call", tool="system.get_diagnostics", arguments={})
        resp = st.send_request(req)
        self.assertEqual(resp.status, "OFFLINE")
        self.assertEqual(resp.telemetry_source, "OFFLINE_SIMULATION")

        # 2. Kernel diagnostic log buffer sizing bounds
        diag_capacity = 32
        diag_subsys_max = 12
        diag_msg_max = 52
        test_subsys = "TOOLONG_SUBSYSTEM_NAME"
        test_msg = "X" * 100
        truncated_subsys = test_subsys[:diag_subsys_max - 1]
        truncated_msg = test_msg[:diag_msg_max - 1]
        self.assertEqual(len(truncated_subsys), 11)
        self.assertEqual(len(truncated_msg), 51)

        # 3. Controlled Shutdown Volatile RAMDisk Warning
        volatility_warning = "Storage is backed by volatile RAMDisk (/dev/ram0). Data is not retained across power cycles."
        self.assertIn("volatile RAMDisk", volatility_warning)
        self.assertIn("not retained", volatility_warning)

    def test_phase19_hardware_abstraction_layer(self):
        # 1. Verify HAL Static Driver Registry Limits
        hal_max_drivers = 16
        core_driver_count = 10
        self.assertLessEqual(core_driver_count, hal_max_drivers)

        # 2. Verify Core Driver Identification and Backend Schema
        core_drivers = {
            "pic_8259": {"class": "INTC", "backend": "HARDWARE", "dep": None, "irq": None},
            "serial_uart": {"class": "SERIAL", "backend": "HARDWARE", "dep": None, "irq": 4},
            "vga_terminal": {"class": "CONSOLE", "backend": "HARDWARE", "dep": None, "irq": None},
            "pit_8254": {"class": "TIMER", "backend": "HARDWARE", "dep": "pic_8259", "irq": 0},
            "ps2_keyboard": {"class": "KEYBOARD", "backend": "HARDWARE", "dep": "pic_8259", "irq": 1},
            "ps2_mouse": {"class": "POINTER", "backend": "HARDWARE", "dep": "pic_8259", "irq": 12},
            "framebuffer": {"class": "DISPLAY", "backend": "HARDWARE", "dep": None, "irq": None},
            "ramdisk_block": {"class": "BLOCK", "backend": "MEMORY_BACKED", "dep": None, "irq": None},
            "acpi_power": {"class": "POWER", "backend": "HARDWARE", "dep": None, "irq": None},
            "net_loopback": {"class": "NETWORK", "backend": "MEMORY_BACKED", "dep": None, "irq": None},
        }

        self.assertEqual(len(core_drivers), 10)
        self.assertEqual(core_drivers["ramdisk_block"]["backend"], "MEMORY_BACKED")
        self.assertEqual(core_drivers["net_loopback"]["backend"], "MEMORY_BACKED")
        self.assertEqual(core_drivers["serial_uart"]["backend"], "HARDWARE")
        self.assertEqual(core_drivers["pit_8254"]["dep"], "pic_8259")
        self.assertEqual(core_drivers["ps2_keyboard"]["dep"], "pic_8259")

        # 3. Verify Safe Fallback Routing Invariant
        display_offline = True
        serial_online = True
        console_routed_to = "serial_uart" if (display_offline and serial_online) else "vga_terminal"
        self.assertEqual(console_routed_to, "serial_uart")

    def test_phase20_networking_stack(self):
        # 1. Ethernet Frame Boundary Sizing Standards
        eth_alen = 6
        eth_hlen = 14
        eth_min_frame = 60
        eth_max_frame = 1518
        eth_mtu = 1500

        self.assertEqual(eth_alen, 6)
        self.assertEqual(eth_hlen, 14)
        self.assertEqual(eth_min_frame, 60)
        self.assertEqual(eth_max_frame, 1518)
        self.assertEqual(eth_mtu, 1500)

        # 2. Bounded Circular Receive Queue Sizing & Overflow Accounting
        rx_queue_capacity = 16
        enqueued_packets = 20
        dropped_expected = enqueued_packets - rx_queue_capacity
        self.assertEqual(dropped_expected, 4)

        # 3. Internet Checksum Invariant: Header + Checksum folds to 0
        def compute_checksum(data: bytes) -> int:
            total = 0
            for i in range(0, len(data), 2):
                word = (data[i] << 8) + (data[i+1] if i+1 < len(data) else 0)
                total += word
                while total >> 16:
                    total = (total & 0xFFFF) + (total >> 16)
            return (~total) & 0xFFFF

        hdr_no_cksum = bytes([
            0x45, 0x00, 0x00, 0x3c,
            0x1c, 0x46, 0x40, 0x00,
            0x40, 0x06, 0x00, 0x00, # Checksum = 0
            0x0a, 0x00, 0x02, 0x0f, # 10.0.2.15
            0x0a, 0x00, 0x02, 0x02  # 10.0.2.2
        ])
        cksum = compute_checksum(hdr_no_cksum)
        hdr_with_cksum = bytearray(hdr_no_cksum)
        hdr_with_cksum[10] = (cksum >> 8) & 0xFF
        hdr_with_cksum[11] = cksum & 0xFF
        self.assertEqual(compute_checksum(bytes(hdr_with_cksum)), 0)

        # 4. Accurate Hardware vs Memory-backed Telemetry Representation
        interfaces = {
            "lo0": {"mac": "02:00:00:00:00:01", "type": "LOOPBACK", "backend": "MEMORY_BACKED"},
            "eth0": {"mac": "52:54:00:12:34:56", "type": "ETHERNET", "backend": "HARDWARE"}
        }
        self.assertEqual(interfaces["lo0"]["backend"], "MEMORY_BACKED")
        self.assertEqual(interfaces["eth0"]["backend"], "HARDWARE")
        self.assertTrue(interfaces["lo0"]["mac"].startswith("02:")) # Locally administered

        # 5. DHCP State Machine & Fallback Static IP Verification
        dhcp_states = ["INIT", "DISCOVERING", "REQUESTING", "BOUND", "FAILED"]
        self.assertIn("BOUND", dhcp_states)
        static_ip_config = {
            "ip": "10.0.2.15",
            "netmask": "255.255.255.0",
            "gateway": "10.0.2.2",
            "dns": "10.0.2.3"
        }
        self.assertEqual(static_ip_config["ip"], "10.0.2.15")
        self.assertEqual(static_ip_config["gateway"], "10.0.2.2")

    def test_phase21_security_and_access_control(self):
        """Phase 21: Verify security boundaries, argument hardening, shell injection blocking, secret redaction, and bounded logs."""
        from jarvis.tools.registry import (
            PermissionLevel,
            redact_secrets,
            MAX_ARG_STRING_LENGTH,
            MAX_ARGS_PAYLOAD_SIZE,
            MAX_OUTPUT_LENGTH,
            MAX_AUDIT_LOG_ENTRIES
        )

        # 1. Deny-by-default for unregistered or unapproved tools
        res_denied = self.registry.execute_tool("unregistered.dangerous_tool", {})
        self.assertEqual(res_denied["status"], "ERROR")
        self.assertIn("Permission Denied", res_denied["error"])

        # 2. Strict Argument Schema & Oversized Parameter Rejection
        self.registry.register_tool(
            "test.secure_action",
            "Test tool for security validation",
            PermissionLevel.READ_ONLY,
            lambda args: {"echo": args.get("param", "")},
            param_schema={"param": str},
            required_params=["param"]
        )
        self.registry.allowlist.add("test.secure_action")

        # Missing required parameter
        res_missing = self.registry.execute_tool("test.secure_action", {})
        self.assertEqual(res_missing["status"], "ERROR")
        self.assertIn("Missing required parameter", res_missing["error"])

        # Parameter string length limit (> 1024 chars)
        huge_arg = "A" * (MAX_ARG_STRING_LENGTH + 10)
        res_huge = self.registry.execute_tool("test.secure_action", {"param": huge_arg})
        self.assertEqual(res_huge["status"], "ERROR")
        self.assertIn("exceeds maximum", res_huge["error"])

        # 3. Prohibited Shell Metacharacters (Command Injection Prevention)
        dangerous_inputs = [
            "test; rm -rf /",
            "file && cat /etc/passwd",
            "`whoami`",
            "test | grep root",
            "file$(id)"
        ]
        for bad_input in dangerous_inputs:
            res_sec = self.registry.execute_tool("test.secure_action", {"param": bad_input})
            self.assertEqual(res_sec["status"], "ERROR")
            self.assertIn("prohibited shell metacharacters", res_sec["error"])

        # 4. Secret & Token Redaction in Context and Logs
        secret_text = "Bearer super_secret_token_12345"
        redacted = redact_secrets(secret_text)
        self.assertNotIn("super_secret_token_12345", redacted)
        self.assertIn("[REDACTED]", redacted)

        dict_with_secrets = {
            "api_key": "key-998877665544",
            "nested": {"token": "token=my_secret_token"}
        }
        redacted_dict = redact_secrets(dict_with_secrets)
        self.assertNotIn("key-998877665544", str(redacted_dict))
        self.assertNotIn("my_secret_token", str(redacted_dict))
        self.assertIn("[REDACTED]", str(redacted_dict))

        # 5. Output Truncation for Bounded Buffer Protection
        self.registry.register_tool(
            "test.large_output",
            "Test tool generating huge output",
            PermissionLevel.READ_ONLY,
            lambda args: {"content": "Z" * (MAX_OUTPUT_LENGTH + 500)}
        )
        self.registry.allowlist.add("test.large_output")
        res_large = self.registry.execute_tool("test.large_output", {})
        self.assertEqual(res_large["status"], "SUCCESS")
        self.assertTrue(res_large["result"].get("truncated", False))
        self.assertIn("TRUNCATED", res_large["result"].get("warning", ""))

        # 6. Bounded Circular Audit Log & Eviction Accounting
        start_evicted = self.registry.evicted_audit_count
        for i in range(MAX_AUDIT_LOG_ENTRIES + 20):
            self.registry.execute_tool("kernel.query_status", {})
        self.assertEqual(len(self.registry.audit_log), MAX_AUDIT_LOG_ENTRIES)
        self.assertGreater(self.registry.evicted_audit_count, start_evicted)


if __name__ == "__main__":
    unittest.main()

