import time
from typing import Optional
from jarvis.bridge.protocol import BridgeRequest, BridgeResponse


class SerialTransport:
    """
    Serial Communication Transport bridging Host AI Companion with JARVIS-OS guest.
    Communicates via line-delimited JSON protocol over COM1 serial / virtual PTY.
    """

    def __init__(self, port: Optional[str] = None, baudrate: int = 115200, timeout: float = 2.0):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.connection = None
        self.is_connected = False
        if port:
            self.connect(port, baudrate, timeout)

    def connect(self, port: str, baudrate: int = 115200, timeout: float = 2.0) -> bool:
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        try:
            import serial
            self.connection = serial.Serial(port, baudrate=baudrate, timeout=timeout)
            self.is_connected = True
            return True
        except (ImportError, Exception):
            self.connection = None
            self.is_connected = False
            return False

    def close(self):
        if self.connection:
            try:
                self.connection.close()
            except Exception:
                pass
        self.connection = None
        self.is_connected = False

    def send_request(self, request: BridgeRequest) -> BridgeResponse:
        start_time = time.time()
        if not self.is_connected or not self.connection:
            # Fallback to offline simulation when live serial transport is unavailable
            return BridgeResponse(
                request_id=request.request_id,
                status="OFFLINE",
                result=None,
                error_message="Serial transport offline. Live guest serial connection not established.",
                execution_time_ms=(time.time() - start_time) * 1000,
                telemetry_source="OFFLINE_SIMULATION"
            )

        try:
            payload = (request.to_json() + "\n").encode("utf-8")
            self.connection.write(payload)
            self.connection.flush()

            line = self.connection.readline()
            elapsed_ms = (time.time() - start_time) * 1000
            if not line:
                return BridgeResponse(
                    request_id=request.request_id,
                    status="TIMEOUT",
                    error_message=f"Serial read timed out after {self.timeout}s",
                    execution_time_ms=elapsed_ms,
                    telemetry_source="OFFLINE_SIMULATION"
                )

            resp_str = line.decode("utf-8", errors="replace").strip()
            resp = BridgeResponse.from_json(resp_str)
            resp.telemetry_source = "LIVE_KERNEL"
            return resp
        except Exception as e:
            elapsed_ms = (time.time() - start_time) * 1000
            return BridgeResponse(
                request_id=request.request_id,
                status="IO_ERROR",
                error_message=f"Serial communication error: {str(e)}",
                execution_time_ms=elapsed_ms,
                telemetry_source="OFFLINE_SIMULATION"
            )

    @staticmethod
    def format_message(payload: dict) -> bytes:
        import json
        return (json.dumps(payload) + "\n").encode("utf-8")

    @staticmethod
    def parse_line(line: str) -> dict:
        import json
        return json.loads(line.strip())

    def dispatch_request(self, request: BridgeRequest, registry) -> BridgeResponse:
        start = time.time()
        res = registry.execute_tool(request.tool, request.arguments)
        elapsed_ms = (time.time() - start) * 1000
        if res.get("status") == "SUCCESS":
            return BridgeResponse(
                request_id=request.request_id,
                status="SUCCESS",
                result=res.get("result"),
                execution_time_ms=elapsed_ms,
                telemetry_source=res.get("result", {}).get("telemetry_source", "OFFLINE_SIMULATION") if isinstance(res.get("result"), dict) else "OFFLINE_SIMULATION"
            )
        else:
            return BridgeResponse(
                request_id=request.request_id,
                status=res.get("status", "ERROR"),
                error_message=res.get("error", "Execution failed"),
                execution_time_ms=elapsed_ms,
                telemetry_source="OFFLINE_SIMULATION"
            )


SerialBridgeTransport = SerialTransport
