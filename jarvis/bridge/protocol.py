import json
import time
from typing import Dict, Any, Optional
from dataclasses import dataclass, asdict

PROTOCOL_VERSION = "1.0"
MAX_MESSAGE_SIZE = 65536  # 64 KB maximum payload limit


@dataclass
class BridgeRequest:
    request_id: int
    action: str
    tool: Optional[str] = None
    arguments: Optional[Dict[str, Any]] = None
    timestamp: float = 0.0
    protocol_version: str = PROTOCOL_VERSION

    def __post_init__(self):
        if self.arguments is None:
            self.arguments = {}
        if self.timestamp == 0.0:
            self.timestamp = time.time()

    def to_json(self) -> str:
        return json.dumps(asdict(self))

    @classmethod
    def from_json(cls, json_str: str) -> "BridgeRequest":
        if not json_str or not isinstance(json_str, str):
            raise ValueError("Input must be a non-empty JSON string")
        if len(json_str) > MAX_MESSAGE_SIZE:
            raise ValueError(f"Message size {len(json_str)} exceeds MAX_MESSAGE_SIZE ({MAX_MESSAGE_SIZE} bytes)")

        data = json.loads(json_str)
        if not isinstance(data, dict):
            raise ValueError("Parsed JSON root must be an object")

        if "request_id" not in data or "action" not in data:
            raise ValueError("Malformed request: missing request_id or action")

        return cls(
            request_id=int(data["request_id"]),
            action=str(data["action"]),
            tool=data.get("tool"),
            arguments=data.get("arguments", {}),
            timestamp=float(data.get("timestamp", time.time())),
            protocol_version=str(data.get("protocol_version", PROTOCOL_VERSION))
        )


@dataclass
class BridgeResponse:
    request_id: int
    status: str
    result: Optional[Any] = None
    error_message: Optional[str] = None
    execution_time_ms: float = 0.0
    telemetry_source: str = "OFFLINE_SIMULATION"
    protocol_version: str = PROTOCOL_VERSION

    def to_json(self) -> str:
        return json.dumps(asdict(self))

    @classmethod
    def from_json(cls, json_str: str) -> "BridgeResponse":
        if not json_str or not isinstance(json_str, str):
            raise ValueError("Input must be a non-empty JSON string")
        if len(json_str) > MAX_MESSAGE_SIZE:
            raise ValueError(f"Message size {len(json_str)} exceeds MAX_MESSAGE_SIZE ({MAX_MESSAGE_SIZE} bytes)")

        data = json.loads(json_str)
        if not isinstance(data, dict):
            raise ValueError("Parsed JSON root must be an object")

        if "request_id" not in data or "status" not in data:
            raise ValueError("Malformed response: missing request_id or status")

        return cls(
            request_id=int(data["request_id"]),
            status=str(data["status"]),
            result=data.get("result"),
            error_message=data.get("error_message"),
            execution_time_ms=float(data.get("execution_time_ms", 0.0)),
            telemetry_source=str(data.get("telemetry_source", "OFFLINE_SIMULATION")),
            protocol_version=str(data.get("protocol_version", PROTOCOL_VERSION))
        )
