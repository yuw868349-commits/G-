from .swiftagent_native import Engine as _NativeEngine, RunResult, Telemetry

__all__ = ["Engine", "RunResult", "Telemetry"]
__version__ = "0.1.0"


class Engine(_NativeEngine):
    def __init__(self, provider: str = "fake", model: str = "gpt-4o-mini",
                 budget_turns: int = 32, api_key: str = "", api_base: str = ""):
        super().__init__(
            provider=provider, model=model,
            api_key=api_key, api_base=api_base,
            budget_turns=int(budget_turns),
        )
        self._budget_turns = int(budget_turns)

    def run(self, task: str, max_turns: int | None = None) -> RunResult:
        if max_turns is None:
            return super().run(task, self._budget_turns)
        return super().run(task, int(max_turns))
