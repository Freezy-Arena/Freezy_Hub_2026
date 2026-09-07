# FMS_TABLE migration and bench validation

## Scope and evidence

Adds runtime role `FMS_TABLE` (numeric ID 2); `redHub` and `blueHub` retain IDs 0 and 1.
Select it on the existing configuration page, or flash over a legacy controller
whose `settings/deviceRole` is `FMS_TABLE` without erasing NVS. This is the same
firmware/environment as the hub: `pio run -e esp32-s3-devkitm-1`.

Read before implementation: `Freezy Estops/AGENTS.md`, `agents/README.md`,
`agents/REFACTOR_PLAN.md`, and `agents/INVESTIGATION.md`. Reference source is
`C:/Users/Capplegate/GitHub/Freezy Estops`, revision `83aff5e`; arena source is
`C:/Users/Capplegate/GitHub/Freezy WorkSpace/freezy-arena`, revision `29795dcb`
(clean working tree when inspected). The reference investigation notes preceded
the user's later capture: **1,016 ms HTTP operation and 1,154 ms stop-sampling gap**.
Those are the comparison baseline, not measurements of this migration.

## Protocol decision

| Route | Inspected implementation | Response and implications |
|---|---|---|
| POST `/api/freezy/eStopState` | `web/alternateIO.go:eStopStatePostHandler`, decodes an array and calls `Plc.SetAlternateIOStopState(channel, state)` | HTTP 200, body `eStop state updated successfully.`; invalid JSON returns 400 |
| WS `/api/plc/websocket`, `setInput` | `handlePLCWebSocketMessage` calls the **same setter** in this arena revision | `plcInputSetSuccess` with count/success; no request ID, sequence, or per-channel acknowledgment; permissive type coercion differs from HTTP decoder |
| WS `/setup/field_testing/websocket` | `web/setup_field_testing.go` subscribes to PLC, arena status, LED changes and periodic LED status | Admin-protected; accepts sound/coil/LED test commands, **not `setInput`** |
| POST `/api/freezy/startMatch` | Calls `Arena.StartMatch()` and ignores its returned error | Always returns 200 after calling it, with legacy body `Field stack light state updated successfully.`; this does **not** confirm a match actually started |
| GET `/api/freezy/field_stack_light` | `GetFieldStackLight()` returns named red/blue/orange/green values | Small JSON object; this semantic mapping is retained instead of assuming legacy coil indices |

`plc/plc.go:SetAlternateIOStopState` assigns `inputs[channel]`;
`GetFieldEStop` returns `!inputs[fieldEStop]`. The HTTP stop and PLC WebSocket
setter are equivalent for valid values **in this inspected fork**, but not all
arena distributions provide either extension. The hub's unacknowledged
`sendInput` helper is not a stop-delivery mechanism.

### WebSocket transport and future server naming

Table stop delivery now uses WebSocket **`setInput`**: both HTTP `eStopState`
and WebSocket `setInput` call **`web.arena.Plc.SetAlternateIOStopState`** in the
inspected server. That method writes the specified PLC input and works for
**all PLC inputs**, not only stops. A remaining server follow-up is to rename
it to **`SetInput`**, updating its interface, implementations, and callers.
No server code is changed by this client migration.

### Transport selected for this migration

The shared `WsManager` connects to public `/api/plc/websocket`. FMS sends exactly:

```json
{"type":"setInput","data":[{"channel":0,"state":false}]}
```

`false` asserts field stop; `true` releases it. A queued transition is retired
only on `{"type":"plcInputSetSuccess","data":{"success":true,"count":1}}`
for the sole outstanding write. `success` must be a JSON boolean and `count`
an integer. Hub telemetry send flags cannot disable FMS stop writes, and the
unacknowledged hub input helpers cannot write while FMS owns this connection.

Start-match requests retain their HTTP contract and run in the separate worker.
Stack lights now follow `plcIoChange.Coils` over WebSocket instead of HTTP polling. Both transports use the same saved arena host and port.
There is no HTTP stop fallback: mixing independent transports would complicate
ordering and acknowledgments. FMS never runs synchronous HTTP login in a WS
callback; an unexpected protected PLC route stays disconnected, retains stop
history, and inhibits starts rather than blocking sampling with authentication.

The PLC route sends PLC and LED notifications, not arenaStatus or redundant
periodic ledStatus frames. The table does not need these to produce its stack
lights; the old `Registers[3]` value only gated console printing. This intentional
subscription change removes unused traffic. The bench fixture still injects
arenaStatus at match rates to stress the parser. No arena code was changed.

## Hardware and output compatibility

| Function | Preserved assignment/behavior |
|---|---|
| Field stop | GPIO **33**, `INPUT`, no new pullup/debounce; wire state is `!digitalRead(33)` |
| Stop channel/polarity | Channel **0** only. GPIO HIGH -> JSON `false` -> field stop asserted. GPIO LOW -> JSON `true` -> released |
| Start | GPIO **34**, `INPUT_PULLUP`; LOW means pressed |
| Other original input pins | GPIO 1, 2, 3, 15, 18, 16 remain INPUT; FMS does not report alliance channels 1–12 |
| W5500 | CS 14, IRQ 10, reset 9, SCK 13, MISO 12, MOSI 11; table uses original **SPI2_HOST**, hubs retain SPI3_HOST |
| Table LEDs | GPIO **47**, WS2812B, GRB, 750 pixels, brightness 15, 900 mW limit, original uncorrected color output |
| Status LEDs | 0 heartbeat; 1 magenta role indicator; 2 alternates dim green/black with parsed WS messages |
| Stack LEDs | Red 3–62, blue 60–119, orange 120–179, green 180–235 (inclusive). Blue is written after red and wins at 60–62, including when blue is off |
| Stack colors | Red `(255,0,0)`, blue `(0,0,255)`, orange `(150,100,0)`, green `(0,255,0)` |

Table startup clears all 750 pixels. Other pixels stay off. The arena coil
mapping was checked against `plc/plc.go`: green=2, orange=3, red=4, blue=5,
matching `src/websocket/coil_map.h`. Each valid `plcIoChange` updates the stack
state, and the main loop renders changed colors without waiting for a 500 ms
poll. There is no HTTP stack poll to overwrite a newer WebSocket state. This
intentionally replaces the legacy named HTTP response with verified coil indices.
Short snapshots or non-boolean stack coils are rejected and preserve the last
valid frame. On disconnect the last frame remains; the reconnect snapshot
resynchronizes it. If several updates arrive before rendering, the latest state
wins rather than replaying old blink phases. Network/server delays can still
lose blink phases; this removes the client's 500 ms polling limitation, not
all possible timing delays. No independent local blink timer is introduced.

LEDs retain their original ranges/colors/overlap and the shared last-frame
comparison, so identical stack updates do not retransmit LED data.

The response heartbeat uses dim white for a valid stop ACK or positive HTTP
response, orange for a failed operation, red while stop transport is offline,
and black phases at 500 ms. A >200 ms
main-service gap overlays red on the success heartbeat for ten seconds. New:
the role indicator becomes solid red for a latched queue/task fault. Per-message
console dumps and repeated held-button logs are removed in favor of summaries.
Malformed JSON is counted rather than blindly coercing a missing message type.

FMS never initializes PCNT or relay channels, particularly the hub relay on
GPIO 34. Hub GPIO 38, LED count/order preferences, relay/counter pins, and LED
control modes remain as before. Table LED settings are fixed and the shared
LED settings page explains them rather than allowing conflicting hub modes.

## Persisted configuration

`src/config/legacy_fms.h` imports before managers initialize. It only imports
when legacy `settings/deviceRole=FMS_TABLE` exists and the hub `role/role` key
does not. Existing hub keys win individually. Original keys are not erased or
rewritten; save the role marker last so an interrupted import can repeat.

| Original key | Destination |
|---|---|
| `settings/deviceRole` | `role/role = 2` |
| `settings/deviceIP` | `network/staticIP` (legacy fallback 10.0.100.240) |
| `settings/deviceGWIP` | `network/staticGW` (legacy fallback 10.0.100.3) |
| `settings/useDHCP` | `network/useDHCP` |
| `settings/arenaIP` | `websocket/arenaHost` |
| `settings/arenaPort` (string) | `websocket/arenaPort` (unsigned port; invalid values fall back to 8080) |
| Hardcoded HTTP `baseUrl` | Intentionally replaced by shared `websocket/arenaHost` and `arenaPort` |

The original web form *read* `deviceGateway` but boot/save used `deviceGWIP`;
migration preserves the effective boot/save key. The original HTTP code ignored
saved arenaIP/arenaPort. **Intentional change requested by the user:** HTTP and
WebSocket always use the same saved `arenaHost` and `arenaPort`; there are no
separate stop HTTP settings. Migrating a nondefault arena address therefore
also redirects HTTP there, fixing the legacy hardcoded destination. Existing hub authentication, send flags, and
LED preferences remain intact. Send Inputs/Registers checkboxes only affect hub
telemetry, never table stop delivery. Subnet /24 and DNS defaults are unchanged.

New settings saves update the hub namespaces, not the rollback copy in
`settings`. Configuration/role changes apply on reboot; the running stop worker
uses an immutable HTTP destination, and hardware role remains fixed until then.
Do not erase flash during migration if retaining NVS is required. Both projects
use the same ESP32-S3 board/framework configuration; confirm the NVS partition
is retained if changing partition tables independently.

## Sampling, retention, ordering, and failure policy

* **Ownership:** priority-3 task on core 1 samples every one RTOS tick (nominal
  1 ms), with no HTTP, Preferences reads/writes, LED rendering, or logging.
  Priority-1 HTTP worker on core 0 owns start-request transport. Arduino loop
  services shared network/WebSocket/web configuration, calls the dedicated
  module's `serviceStops`, and renders snapshots through LedManager.
  WebSocketsClient has one owner (the main loop); background tasks never call it.
  Only short critical sections protect queue and status; none enclose network
  operations. Ethernet link/IP state is atomic. Boot does not wait for Ethernet.
* **Initial state and transitions:** enqueue the initial snapshot, then every
  sampled state change immediately. No stop debounce, batching, or 500 ms
  telemetry schedule. Pulses entirely between samples cannot be captured;
  verify actual sampling gaps on hardware. This is not a GPIO interrupt recorder.
* **Retention:** 256 FIFO entries in RAM, including the in-flight head. Each
  has a monotonic sequence and sample timestamp. Repeated identical samples
  allocate nothing. No age-based expiry/coalescing of stops. RAM history is lost
  on reboot/power loss; no NVS writes are performed in the sampling path.
* **Acknowledgments:** one `setInput` in flight, including idle-state refreshes.
  Only the typed `plcInputSetSuccess` response described above retires the head.
  Send success alone does not count. No ACK within 500 ms, an invalid ACK,
  server error, send failure, or disconnect leaves the transition queued.
  The ACK state machine is `src/websocket/input_ack.h`.
* **Retries and connection boundaries:** ACKs have no request ID. After a
  timeout, invalid ACK, or partial/failed send, **close the old connection before
  retrying**; a late ACK from its TCP stream cannot acknowledge the next head.
  On disconnect retain the head and retry after reconnect. The existing retry
  backoff remains 50, 100, 200, 400, then 1,000 ms capped; WS reconnection uses
  the existing 3,000 ms interval, so effective retry time also includes reconnect.
  Reset backoff after ACK. Ignore unsolicited ACKs when no request is pending.
  The server contract is one ACK per request; arbitrary delayed duplicate ACKs
  on a still-valid connection cannot be correlated without server request IDs.
* **Retention on outage:** keep sampling with no stop expiration. On recovery
  drain FIFO before idle-state refresh. An applied write whose ACK was lost can
  be repeated: the setter is idempotent, but there is no exactly-once transaction
  or server-side persistence guarantee. Refresh the last sampled state every
  100 ms when empty because `ResetEstops()` can overwrite inputs without a GPIO
  edge. Refreshes also wait for ACK and never bypass queued transitions.
* **Remote visibility:** after an acknowledged assertion, wait at least 100 ms
  before sending a release. This intentional hold prevents immediately replaying
  a rapid assertion/release into the same arena update period. It is not proof
  that the arena observed/acted on the assertion; test the actual arena state
  machine. Strict order and this hold limit backlog drain rate.
* **Overflow:** freeze the retained FIFO, count every additional unretained
  transition, and latch FAULT. Continue sampling latest state and timing. Inhibit
  starts; repeatedly send channel 0 `false` with the same acknowledgment/retry
  policy, nominally every 100 ms after successful responses. Never replay queued
  releases after overflow. A request already in flight may finish first. The
  red role LED and every diagnostic report expose the fault and loss count.
  Task allocation failure also latches FAULT; a failed worker cannot deliver it.
* **Recovery from overflow:** keep the bench/field stopped, investigate the
  overflow, verify physical stop assertion and working arena transport, then
  explicitly reboot to clear the latch. This discards the frozen RAM history;
  after reboot a fresh physical snapshot is sent. There is no automatic fault
  clearance or claim of unlimited retention during indefinite outages.
* **Start:** require 30 ms stable release to arm and 30 ms stable LOW to trigger;
  held-at-boot never starts. One request per release/press, never repeated while
  held. One pending request, 500 ms expiry, canceled by a sampled stop, fault, or
  observed Ethernet/WS loss. Dispatch only with empty stop history, released
  stop, connected WebSocket, and no pending stop ACK or outstanding stop failure. Never retry a start, including lost response;
  require release/press again. This avoids delayed starts after arena outages.
  The HTTP 200 is recorded as request response, not confirmation of match start.
* **HTTP waits:** connect timeout 250 ms, read timeout 500 ms, no redirects, no
  keepalive, at most 512 response bytes. These are library operation timeouts,
  not a guaranteed end-to-end real-time deadline. An in-flight start call
  no longer holds up stop delivery: WS stop servicing runs separately in the main
  loop. Measure both HTTP duration and WS ACK latency. WebSocket sends and LED
  output share the main loop; the high-priority sampler remains independent. WebSocket's
  own TCP reconnect/handshake can still produce main-service gaps during outages;
  there is no claim that all network failures meet the steady-state WS target.

## Diagnostics

Routine debug output is disabled by `DEBUG_SERIAL=false`: per-message ping,
LED-mode, match-reset, and DMX receive logs are gated. Startup/connection events
and fault/error messages remain enabled. The hub status summary and the single
FMS timing summary remain enabled at 5,000 ms independently of the debug flag.
The separate periodic print in `ws_manager` is retained but disabled by
`_debugSerial=false`; enable debugging to restore its five-second summary.

`[FMS TIMING]` reports every five seconds; counters and maxima
are **since boot**, unlike the original windowed diagnostics. Restart a bench
case to get independent maxima. `sample_gap_us` is in microseconds; all `_ms`
fields and WebSocket gap/work are milliseconds. HTTP includes body read/cleanup
for start calls only. `stop_sent` counts tracked WS send attempts including
refreshes; `stop_fail` counts failed sends/ACKs/disconnects; `retry` counts stop
failures requiring retry. `stop_rtt_ms` is maximum send-to-result time, including
failed requests. `fail` and `last_http` describe HTTP only. These counters keep
HTTP delay separate from WS stop delivery.
`ack_max_ms` measures sample-to-verified-ACK for actual queued transitions;
refreshes do not increment `ack`. `oldest_ms` includes outage/retry wait.
`observed`, `ack`, `queue`, `high`, `unretained`, `FAULT`, HTTP/retry and start
counters make delayed or lost history visible. `led_max_ms` times LED show;
`loop_gap_ms` and WS gap/work reveal remaining main-service stalls.

The firmware intentionally does not print credentials or raw high-rate message
payloads. Capture serial at 115200 alongside timestamped GPIO and arena logs.

## Automated checks

Ask the user before running simulation tests or the combined runner below,
as required by `AGENTS.md`. They are not authorized automatically by a code change.

```powershell
pio run -e esp32-s3-devkitm-1
./tools/fms_bench/run_checks.ps1
```

Policy tests use C++14 `static_assert` to **evaluate the production policy at
compile time**, including 200 rapid transitions retained during an outage,
wrong acknowledgments, retry-head retention, ring wrap, explicit overflow,
held/bouncing starts, clock rollover, polarity/mapping, and retry schedule. They also evaluate the production WS ACK state machine
for one in-flight request, timeout/late ACK, disconnect, typed response validation,
and connection reset before retry.
They need no native executable or attached ESP32. Python tests exercise the
mock over actual local HTTP/WebSocket sockets, including a 1,016 ms delayed HTTP
response while notifications and WS stop ACKs continue, plus missing/delayed/
invalid ACKs across socket reconnects. These tests verify policy and fixture,
not FreeRTOS scheduling, W5500 electrical behavior, or actual arena stop action.

## Hardware bench procedure

Use an isolated bench arena and controller. Set the shared **arena host and port**
to the bench computer's reachable IP and port for both HTTP and WebSocket. Do not point the
mock tests at an operating competition arena. Record firmware revision, board,
role, NVS values (exclude secrets), Ethernet settings, and LED load.

```powershell
python tools/fms_bench/mock_arena.py --bind 0.0.0.0 --port 8080 --log fms-bench.jsonl
```

The mock defaults to 50 notifications/sec **per topic** (150 total). Control it
from a second terminal; only these requests go to the mock's `/control`:

```powershell
Invoke-RestMethod http://127.0.0.1:8080/control -Method Post -ContentType application/json -Body '{"delay_ms":1016,"notifications_hz":100}'
Invoke-RestMethod http://127.0.0.1:8080/control -Method Post -ContentType application/json -Body '{"delay_ms":0,"fail_stops":3}'
Invoke-RestMethod http://127.0.0.1:8080/control -Method Post -ContentType application/json -Body '{"drop_stop_responses":1}'
Invoke-RestMethod http://127.0.0.1:8080/control -Method Post -ContentType application/json -Body '{"stop_ack_delay_ms":1500}'
Invoke-RestMethod http://127.0.0.1:8080/control -Method Post -ContentType application/json -Body '{"stop_ack_delay_ms":0,"invalid_stop_acks":1}'
```

Drive GPIO 33 using a 3.3 V signal generator or a second MCU with common ground,
with the existing stop wiring disconnected for this test. HIGH asserts stop.
Use a switch to ground on GPIO 34 for start. Capture GPIO 33/34 with a logic
analyzer; don't infer sample counts just from a hand-operated switch's bounce.

| Case | Stimulus | Required observations |
|---|---|---|
| Migration/configuration | Start with legacy FMS NVS and nondefault static/network/arena values; flash without erase; reboot twice | Role/pins correct; one import; saved arena destination retained and used for both WS and HTTP; original keys unchanged; edited shared settings survive reboot; invalid form saves nothing |
| Hub regression | Boot redHub then blueHub with their saved settings | GPIO 38/count/order unchanged; relay/counters and telemetry still work; no table tasks/HTTP; selecting a new role does not change live pin use before reboot |
| Baseline | Stable released stop, LEDs active, 5 minutes | One initial transition ACK; repeated 100 ms state refresh; no new history for unchanged input; no unretained events/FAULT |
| Rapid transitions | 100 HIGH/LOW pairs at 10 ms per level; repeat at 2 ms; keep total pending below 256 | `observed` increases by 200; eventual ACK of all 200 in order. Collapse adjacent equal values in applied mock records to ignore refresh/retry duplicates; compare to generator trace. Release follows assertion ACK by >=100 ms. Sub-sampling pulses explicitly outside guarantee |
| Held start | Release >=30 ms; hold LOW 10 seconds; release/press again; reboot with LOW held | Exactly one POST per deliberate press; zero repeats during hold; none while held at boot. Try <30 ms bounce and short release bounce; no re-arm |
| Stop vs start | Assert stop while start pending; hold stop and press start; fill stop backlog then press start | Start canceled/inhibited or expires. A stop arriving during an in-flight HTTP start is sampled and delivered over WS independently; record its ACK latency |
| Slow HTTP | Set delay_ms=1016, then 1500 for 30 seconds while toggling stops and running notification traffic; restore 0 | Sampler stays active and WS stop ACKs continue despite slow HTTP; queue drains without waiting for HTTP completion. Separate HTTP duration from stop RTT and service gaps |
| Arena failure | fail_stops=3, then stop mock for 30 seconds and restart with the same log; generate <=200 changes | FIFO/head retained, retries back off, ACK count stops on failure; timed-out/error sockets reset before retry; no starts replayed after outage, eventual ordered drain and state refresh |
| Lost response | drop_stop_responses=1 during a transition | ACK timeout closes WS; same state retried on a fresh connection before later transitions; no exactly-once assumption. Also inject stop_ack_delay_ms=1500 and invalid_stop_acks=1; late/invalid ACKs must not retire a later head. Restore delay to 0 to drain |
| Overflow | Arena down; generate >256 changes | Red role indicator; FAULT=1; unretained count increases explicitly; frozen FIFO does not drain releases. On recovery only `false` stop requests; no start. Clear using the explicit stopped-bench recovery procedure |
| Ethernet | Boot cable unplugged; exercise inputs, plug in; repeat link loss/recovery with DHCP and static IP | Diagnostics/sampling continue before IP; history survives link loss; addresses reacquired; HTTP and WS recover; no held-button start on reconnect |
| Stack/response | Toggle coils 2?5 independently through plcIoChange; flash green at 100?250 ms phases; send short/non-boolean coil snapshots | Follow each received phase without 500 ms polling; original ranges/colors/overlap preserved; malformed/short snapshot retains prior lights; reconnect resynchronizes; unchanged frames not resent |
| Match-rate/arena | 100 per topic/sec for 10 minutes, with stack changes and rapid stop input; then actual isolated arena at real match rate | Connected-state WS gap target <=100 ms; input gap target <=10,000 us, including delayed HTTP. Compare blocked-listener warnings with controller connected/disconnected and correlate timing; confirm arena actually asserts field stop, not only that setInput returns a success ACK |

Check a saved steady-state capture:

The checker below expects both diagnostic lines, including `[WS TIMING]`.
Enable debugging when collecting a capture for it. With debugging disabled,
check `sample_gap_us` and `loop_gap_ms` in the FMS summary directly.

```powershell
python tools/fms_bench/check_timing.py serial-capture.txt
```

The checker rejects missing diagnostics, sample gaps >10 ms, WS gaps >100 ms,
any overflow/fault. These are bench acceptance targets, **not measured guarantees**.
It is expected to reject the deliberate overflow case; inspect that separately.
Outage WS connect stalls must be separated from connected-state notification
tests. Compare JSONL application order and GPIO traces separately; the timing
checker alone cannot prove stop delivery or arena action.

## Validation record

The `esp32-s3-devkitm-1` firmware build passed, all compile-time policy assertions
passed with `-Wall -Wextra -Werror`, and all eight Python fixture/timing tests
passed. The build still emits the existing PCNT driver deprecation warning.

The following user-reported hardware results are the **HTTP transport baseline**
(commit `61cf390`), not validation of the subsequent WS transport change. Repeat
start/stop, held start, stack coil flashing, and outage/reconnect tests on the new firmware, including
lost ACK and delayed ACK cases. WS transport has not been flashed by the agent.

User-reported baseline hardware validation:

* FMS_TABLE start and stop work.
* Hub hardware regression: LEDs, relays, and counters all work. The specific
  hub role(s) tested were not identified.
* The supplied FMS_TABLE serial capture shows maximum sampling gap 3,341 us,
  WebSocket service gap 22 ms, LED output 20 ms, and HTTP operation 518 ms.
  All seven recorded states (including the initial snapshot) were acknowledged;
  queue high-water was three, with no overflow or unretained transitions and
  no WebSocket disconnects. One HTTP failure/retry was recorded.
* The 2,080 ms acknowledgment maximum already appeared with the initial
  snapshot; it includes startup readiness and is cumulative, not a measurement
  of each subsequent stop. The capture ends with an incomplete diagnostic line.

* User subsequently reported all requested outage-recovery tests passed:
  Ethernet unplug/reconnect with stop changes while offline, arena shutdown
  and restart, holding START through an outage without a delayed start, and
  booting without Ethernet followed by connection. This includes the follow-up
  check with multiple stop changes queued during the outage.
* The supplied recovery capture independently shows an initial state retained
  for 36,073 ms until networking returned, successful WebSocket reconnection,
  and queue drain. Maximum sampling gap was 1,475 us while offline and 2,647 us
  after recovery; HTTP peaked at 48 ms. Both recorded states were acknowledged,
  with no overflow or unretained transitions. The acknowledgment maximum
  includes the outage, not just the HTTP operation.

Controlled rapid-transition ordering verified against GPIO/request traces,
intentional overflow, and sustained match-rate traffic remain unconfirmed;
the outage test report does not establish those separate stress-test results
or resolution of the arena's blocked-listener warnings. Hardware tests were
performed by the user; no controller was flashed or live arena contacted by
the coding agent during implementation.
