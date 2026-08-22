from dataclasses import dataclass


@dataclass(frozen=True)
class Request:
    """One inbound gateway event.

    timestamp is milliseconds since epoch (or any monotonic ms clock).
    The engine never calls time.time(); tests inject timestamps.
    cost is how many quota units this event consumes (weighted limiter).
    """

    user_id: str
    endpoint: str
    timestamp: int
    cost: int = 1
