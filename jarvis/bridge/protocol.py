import json
import time
from typing import Dict, Any, Optional
from dataclasses import dataclass, asdict


@dataclass
class BridgeRequest:
    request_id: int
    action: str
    tool: Optional[str] = None
    arguments: Dict[str, Any] = None
    timestamp: float = 0.0

    def to_json(self) -> str:
        d = asdict(self)
        if self.timestamp == 0.0:
            d["timestamp"] = time.time()
        return json.dumps(d)

    @classmethod
    def from_json(cls, json_str: str) -> "BridgeRequest":
        data = json.loads(json_str)
        return cls(
            request_id=data.get("request_id", 0),
            action=data.get("action", "query"),
            tool=data.get("tool"),
            arguments=data.get("arguments", {}),
            timestamp=data.get("timestamp", time.time())
        )


@dataclass
class BridgeResponse:
    request_id: int
    status: str
    result: Optional[Any] = None
    error_message: Optional[str] = None
    execution_time_ms: float = 0.0

    def to_json(self) -> str:
        return json.dumps(asdict(self))

    @classmethod
    def from_json(cls, json_str: str) -> "BridgeResponse":
        data = json.loads(json_str)
        return cls(
            request_id=data.get("request_id", 0),
            status=data.get("status", "ERROR"),
            result=data.get("result"),
            error_message=data.get("error_message"),
            execution_time_ms=data.get("execution_time_ms", 0.0)
        )
