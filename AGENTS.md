# Repository Rules

## Scope

- Work against the current repository and current `.ioc`; do not rely on old chat conclusions.
- For a new conversation, read `docs/current_status.md` first, then load only the task-specific
  authoritative documents listed in `docs/README.md`.
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
- Do not commit generated host artifacts such as `__pycache__/`, `*.pyc`, test executables,
  IDE workspaces, or build output. `_output/README.md` is the only tracked file under `_output/`.
- Treat `Drivers/` and `Middlewares/` as vendored code. Change them only for an explicit,
  reviewed vendor patch; normal application work belongs in the hand-written directories.

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
- Keep build/test evidence in `docs/verification.md`; do not duplicate hardware status in
  protocol documents or infer a hardware pass from source review.

## Documentation

- Keep `docs/current_status.md` concise: current baseline, verified facts, open verification,
  and immediate next steps only.
- Preserve information during merges and reorganizations. Content missing from one side is not
  authorization to delete content present on the other side.
- Delete documentation only when it is proven obsolete, conflicts with current code/configuration,
  or has been fully migrated to a linked authoritative location.
- Preserve dated hardware evidence and its version scope. New results supplement or supersede the
  current status but do not erase useful historical evidence.
