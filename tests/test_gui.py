import os
import sys
import unittest
from fastapi.testclient import TestClient

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from jarvis.gui.server import app


class TestJARVISGUI(unittest.TestCase):

    def setUp(self):
        self.client = TestClient(app)

    def test_serve_index(self):
        res = self.client.get("/")
        self.assertEqual(res.status_code, 200)
        self.assertIn("J.A.R.V.I.S.", res.text)
        self.assertIn("System Vitality", res.text)

    def test_get_status(self):
        res = self.client.get("/api/status")
        self.assertEqual(res.status_code, 200)
        data = res.json()
        self.assertEqual(data["kernel"], "x86-64")
        self.assertEqual(data["status"], "ONLINE")
        self.assertIn("memory", data)
        self.assertIn("registers", data)

    def test_get_processes(self):
        res = self.client.get("/api/processes")
        self.assertEqual(res.status_code, 200)
        data = res.json()
        self.assertGreaterEqual(data["count"], 1)
        # Verify kernel_idle exists
        pids = [p["pid"] for p in data["processes"]]
        self.assertIn(0, pids)

    def test_create_process(self):
        res = self.client.post("/api/processes/create", json={"name": "test_daemon", "priority": 1, "is_thread": False})
        self.assertEqual(res.status_code, 200)
        data = res.json()
        self.assertEqual(data["status"], "SUCCESS")
        self.assertEqual(data["process"]["name"], "test_daemon")

    def test_storage_lifecycle(self):
        # 1. List files (kernel.cfg must exist)
        res = self.client.get("/api/storage/files")
        self.assertEqual(res.status_code, 200)
        data = res.json()
        filenames = [f["name"] for f in data["files"]]
        self.assertIn("kernel.cfg", filenames)

        # 2. Read kernel.cfg
        res_read = self.client.get("/api/storage/file/kernel.cfg")
        self.assertEqual(res_read.status_code, 200)
        self.assertIn("JARVIS-OS", res_read.json()["content"])

        # 3. Create a test file
        res_write = self.client.post("/api/storage/file", json={"name": "gui_test.txt", "content": "HUD telemetry active\n"})
        self.assertEqual(res_write.status_code, 200)

        # 4. Verify readback
        res_verify = self.client.get("/api/storage/file/gui_test.txt")
        self.assertEqual(res_verify.status_code, 200)
        self.assertEqual(res_verify.json()["content"], "HUD telemetry active\n")

        # 5. Delete file
        res_del = self.client.delete("/api/storage/file/gui_test.txt")
        self.assertEqual(res_del.status_code, 200)

    def test_cpu_scheduling(self):
        tasks = [
            {"id": 1, "arrival_time": 0, "burst_time": 6},
            {"id": 2, "arrival_time": 1, "burst_time": 8},
            {"id": 3, "arrival_time": 2, "burst_time": 3},
            {"id": 4, "arrival_time": 3, "burst_time": 4}
        ]
        # FCFS
        res_fcfs = self.client.post("/api/algorithms/cpu-sched", json={"algorithm": "fcfs", "tasks": tasks, "quantum": 3})
        self.assertEqual(res_fcfs.status_code, 200)
        data_fcfs = res_fcfs.json()
        self.assertEqual(data_fcfs["avg_turnaround"], 13.0)
        self.assertEqual(data_fcfs["avg_waiting"], 7.75)

        # SJF
        res_sjf = self.client.post("/api/algorithms/cpu-sched", json={"algorithm": "sjf", "tasks": tasks, "quantum": 3})
        self.assertEqual(res_sjf.status_code, 200)
        data_sjf = res_sjf.json()
        self.assertEqual(data_sjf["avg_turnaround"], 10.75)
        self.assertEqual(data_sjf["avg_waiting"], 5.5)

        # Round Robin
        res_rr = self.client.post("/api/algorithms/cpu-sched", json={"algorithm": "rr", "tasks": tasks, "quantum": 3})
        self.assertEqual(res_rr.status_code, 200)
        data_rr = res_rr.json()
        self.assertEqual(data_rr["avg_turnaround"], 14.5)
        self.assertEqual(data_rr["avg_waiting"], 9.25)

    def test_page_replacement_and_belady(self):
        # Belady Anomaly test: 3 frames vs 4 frames under FIFO
        res_belady = self.client.get("/api/algorithms/belady")
        self.assertEqual(res_belady.status_code, 200)
        data = res_belady.json()
        self.assertEqual(data["frames_3_faults"], 9)
        self.assertEqual(data["frames_4_faults"], 10)
        self.assertTrue(data["anomaly_confirmed"])

    def test_banker_safety(self):
        alloc = [[0, 1, 0], [2, 0, 0], [3, 0, 2], [2, 1, 1], [0, 0, 2]]
        max_m = [[7, 5, 3], [3, 2, 2], [9, 0, 2], [2, 2, 2], [4, 3, 3]]
        avail = [3, 3, 2]
        res = self.client.post("/api/algorithms/banker/safety", json={
            "allocation": alloc,
            "max_matrix": max_m,
            "available": avail
        })
        self.assertEqual(res.status_code, 200)
        data = res.json()
        self.assertTrue(data["is_safe"])
        self.assertEqual(len(data["safe_sequence"]), 5)

    def test_concurrency_actions(self):
        # Produce item
        res_p = self.client.post("/api/algorithms/sync/action", json={"action": "produce"})
        self.assertEqual(res_p.status_code, 200)
        self.assertEqual(res_p.json()["status"], "SUCCESS")

        # Consume item
        res_c = self.client.post("/api/algorithms/sync/action", json={"action": "consume"})
        self.assertEqual(res_c.status_code, 200)
        self.assertEqual(res_c.json()["status"], "SUCCESS")

    def test_companion_query(self):
        res = self.client.post("/api/companion/query", json={"prompt": "What is the kernel status?"})
        self.assertEqual(res.status_code, 200)
        data = res.json()
        self.assertEqual(data["status"], "SUCCESS")
        self.assertEqual(data["tool_called"], "kernel.query_status")
        self.assertIn("JARVIS", data["llm_response"])

    def test_shell_execution(self):
        # Test help
        res_help = self.client.post("/api/shell/exec", json={"command": "help"})
        self.assertEqual(res_help.status_code, 200)
        self.assertIn("JARVIS-OS SHELL COMMANDS", res_help.json()["output"])

        # Test status
        res_status = self.client.post("/api/shell/exec", json={"command": "status"})
        self.assertEqual(res_status.status_code, 200)
        self.assertIn("JARVIS-OS KERNEL STATUS", res_status.json()["output"])


if __name__ == "__main__":
    unittest.main()

