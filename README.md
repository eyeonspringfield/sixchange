# Sixchange

Sixchange is a simulated electronic exchange focused on price-time priority order matching, deterministic behavior, and low-latency systems design. It provides a matching engine and order-entry protocol intended to grow into a network-accessible exchange simulator with live market data and algorithmic trading support.

## Features

- Price-time-priority limit order book
- Limit order matching with partial fills and multi-level sweeps
- Order cancellation
- GFD TIF support
- Limit order type support
- Event-driven engine output
- Text-based order-entry protocol
- Configurable order-book capacity
- Async logging

## Example

As an example, two crossing orders can be entered as:

```
> N 1001 AAPL S L GFD 100 20
ACCEPTED 1001 1

> N 1002 AAPL B L GFD 100 10
ACCEPTED 1002 2
```

Here the second order crosses the resting sell order at 100, producing a trade for 10 units and leaving 10 units of the original sell order resting.

At `Debug` log level, the corresponding engine activity looks like:
```
TextCodec::decode New order decoded client_order_id=1001, symbol=AAPL, side=SELL, type=LIMIT, tif=GFD, price=100, quantity=20
OrderGateway::on_engine_event New order accepted sequence=1, client_order_id=1001, order_id=1, symbol_id=0
OrderBook::rest Order rested sequence=1, order_id=1, client_order_id=1001, side=SELL, price=100, remaining_quantity=20
TextCodec::decode New order decoded client_order_id=1002, symbol=AAPL, side=BUY, type=LIMIT, tif=GFD, price=100, quantity=10
OrderGateway::on_engine_event New order accepted sequence=2, client_order_id=1002, order_id=2, symbol_id=0
OrderBook::match_buy Trade executed aggressing_order_id=2, resting_order_id=1, price=100, quantity=10
```

## Order entry protocol

Sixchange currently exposes a simple text-based order-entry protocol through the command-line interface.

A new order uses the following format:
```
N <client-order-id> <symbol> <side> <order-type> <time-in-force> <price> <quantity>
```

A cancel request uses the following format:
```
C <client-order-id> <symbol>
```

The following fields can have the following values:

| Field             | Value(s) |
|-------------------|----------|
| `<symbol>`        | `AAPL`   |
| `<side>`          | `B`, `S` |
| `<order-type>`    | `L`      |
| `<time-in-force>` | `GFD`    |

## Building + testing

Sixchange requires a C++23-capable compiler, CMake, and Ninja. GCC 14 or newer is recommended.

### Debug

Configure and build:

```bash
cmake --preset debug
cmake --build --preset debug
```

Run the CLI:
```bash
./build/debug/sixchange
```

### Release

Sixchange provides two optimized Release configurations.

#### Portable release

```bash
cmake --preset release
cmake --build --preset release
```

The portable Release build uses `-O3` optimization and has interprocedural/link-time optimization (IPO/LTO) enabled.

#### Native release

```bash
cmake --preset release-native
cmake --build --preset release-native
```

The native Release build adds `-march=native` and `-mtune=native` to the optimizations used by the portable Release configuration. This means the resulting binary is **not guaranteed to be portable**!
### Tests

Sixchange has an extensive suite of GTest based unit tests, runnable in several configurations:

#### Debug test workflow

```bash
cmake --workflow --preset run-tests
```

#### Sanitizer suite workflow (ASan + UBSan)

```bash
cmake --workflow --preset run-asan-tests
```

#### Optimized native test suite workflow

```bash
cmake --workflow --preset run-release-native-tests
```

## Roadmap

### Matching engine

- Order amendment with priority retention for quantity reductions
- Order replacement with priority loss
- IOC and FOK time-in-force support
- Market orders

### Exchange simulation

- Multiple trading states, including opening/closing auctions, pre-open,
  post-close, and OBTRD
- Scheduled market opening and closing
- ULLL / volatility-auction-related stability mechanisms

### Infrastructure

- Network-accessible order entry
- Live market-data feed
- Web portal for order entry and market visualization
- Support for algorithmic market participants

## Design goals

Sixchange is primarily a systems-programming and exchange engine project. The core aims are:

- Deterministic matching behavior
- Price-time priority
- Predictable memory usage
- Minimal allocation in the latency-critical path
- Clear separation between order-entry, matching, and downstream event consumers
- Suitability for later low-latency optimization