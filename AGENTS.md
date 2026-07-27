# Repository Rules

## Scope

- Work against the current repository and current `.ioc`; do not rely on old chat conclusions.
- Keep changes scoped to the requested feature. Do not create speculative modules or documents.
- Do not commit or push unless explicitly requested.
- Resolve conflicts in this order: current code and `.ioc`, wire protocol, responsibility-specific
  documentation, then examples and external references.

## Code Structure

- CubeMX generated code stays in `Core/`, `Drivers/`, `Middlewares/`, and `Application/`.
- Hand-written code belongs in the existing `app`, `board`, `bsp`, `components`,
  `communication`, `infrastructure`, `modules`, `rtos`, `tests`, or `config` boundary.
- Create a directory or target only for a real ownership or dependency boundary.
- Do not extract one-use helpers when a few clear lines in the caller are easier to read.
- Defend user input, external protocols, persistent data, and hardware/resource boundaries.
  Do not add redundant checks for hardcoded constants or values already guaranteed locally.

## Firmware Safety

- Startup and every failure path must leave all motor PWM outputs at zero.
- ISR code must remain bounded. Do not parse protocols, print, erase Flash, or run PID in ISR.
- `control_task` must not perform Console, LCD, QSPI erase/write, RTC display, or text telemetry.
- Dangerous target tests require stopped control, zero PWM, exclusive ownership, and explicit confirmation.
- CubeMX files may be edited manually only inside `USER CODE BEGIN/END` blocks.

## Verification

- Run the smallest relevant build or check for the change.
- Record hardware results only after an actual board test.
- Use `PASS` only with evidence; otherwise use `READY`, `IMPLEMENTED`, or `NOT VERIFIED`.
- Update only the authoritative document for the changed information.
