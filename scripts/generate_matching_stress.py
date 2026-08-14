#!/usr/bin/env python3

import argparse
import random
import sys


class CommandGenerator:
    def __init__(self, symbol: str) -> None:
        self.symbol = symbol
        self.next_client_order_id = 1

        self.order_count = 0
        self.cancel_count = 0

        self.buffer: list[str] = []

    def flush(self) -> None:
        if self.buffer:
            sys.stdout.write("".join(self.buffer))
            self.buffer.clear()

    def emit(self, line: str) -> None:
        self.buffer.append(line)

        if len(self.buffer) >= 8192:
            self.flush()

    def new_order(
            self,
            side: str,
            price: int,
            quantity: int
    ) -> int:
        client_order_id = self.next_client_order_id
        self.next_client_order_id += 1

        self.emit(
            f"N {client_order_id} "
            f"{self.symbol} "
            f"{side} L GFD "
            f"{price} {quantity}\n"
        )

        self.order_count += 1

        return client_order_id

    def cancel(self, client_order_id: int) -> None:
        self.emit(
            f"C {client_order_id} {self.symbol}\n"
        )

        self.cancel_count += 1

    @property
    def command_count(self) -> int:
        return self.order_count + self.cancel_count


def scenario_exact_cross(
        generator: CommandGenerator,
        base_price: int,
        rng: random.Random,
) -> None:
    price = base_price + rng.randrange(20)
    quantity = rng.randint(1, 100)

    # Resting sell, aggressive buy.
    generator.new_order("S", price, quantity)
    generator.new_order("B", price, quantity)

    # Resting buy, aggressive sell.
    quantity = rng.randint(1, 100)

    generator.new_order("B", price, quantity)
    generator.new_order("S", price, quantity)


def scenario_partial_fill(
        generator: CommandGenerator,
        base_price: int,
        rng: random.Random,
) -> None:
    price = base_price + rng.randrange(20)
    quantity = rng.randint(10, 100)

    #
    # Resting ask is partially filled.
    #
    resting_sell = generator.new_order(
        "S",
        price,
        quantity * 2
    )

    generator.new_order(
        "B",
        price,
        quantity
    )

    # Remaining half of resting sell is still live.
    generator.cancel(resting_sell)

    #
    # Aggressive buy is partially filled and rests.
    #
    generator.new_order(
        "S",
        price,
        quantity
    )

    aggressive_buy = generator.new_order(
        "B",
        price,
        quantity * 2
    )

    # Remaining half of aggressive buy is live.
    generator.cancel(aggressive_buy)

    #
    # Resting bid is partially filled.
    #
    resting_buy = generator.new_order(
        "B",
        price,
        quantity * 2
    )

    generator.new_order(
        "S",
        price,
        quantity
    )

    generator.cancel(resting_buy)

    #
    # Aggressive sell is partially filled and rests.
    #
    generator.new_order(
        "B",
        price,
        quantity
    )

    aggressive_sell = generator.new_order(
        "S",
        price,
        quantity * 2
    )

    generator.cancel(aggressive_sell)


def scenario_fifo(
        generator: CommandGenerator,
        base_price: int,
        depth: int,
        rng: random.Random,
) -> None:
    price = base_price + 25

    #
    # Many asks at one price, consumed FIFO.
    #
    total_quantity = 0

    for _ in range(depth):
        quantity = rng.randint(1, 100)

        generator.new_order(
            "S",
            price,
            quantity
        )

        total_quantity += quantity

    generator.new_order(
        "B",
        price,
        total_quantity
    )

    #
    # Many bids at one price, consumed FIFO.
    #
    total_quantity = 0

    for _ in range(depth):
        quantity = rng.randint(1, 100)

        generator.new_order(
            "B",
            price,
            quantity
        )

        total_quantity += quantity

    generator.new_order(
        "S",
        price,
        total_quantity
    )


def scenario_sweep(
        generator: CommandGenerator,
        base_price: int,
        depth: int,
        rng: random.Random,
) -> None:
    #
    # Ask ladder:
    #
    # 100
    # 101
    # 102
    # ...
    #
    # One aggressive buy consumes the whole ladder.
    #
    total_quantity = 0

    for level in range(depth):
        quantity = rng.randint(1, 100)

        generator.new_order(
            "S",
            base_price + level,
            quantity
        )

        total_quantity += quantity

    generator.new_order(
        "B",
        base_price + depth - 1,
        total_quantity
    )

    #
    # Bid ladder:
    #
    # high
    # ...
    # low
    #
    # One aggressive sell sweeps downward.
    #
    total_quantity = 0

    for level in range(depth):
        quantity = rng.randint(1, 100)

        generator.new_order(
            "B",
            base_price + level,
            quantity
        )

        total_quantity += quantity

    generator.new_order(
        "S",
        base_price,
        total_quantity
    )


def scenario_limit_stop(
        generator: CommandGenerator,
        base_price: int,
) -> None:
    #
    # Buy can consume 100 but must stop before 102.
    #
    low_ask = generator.new_order(
        "S",
        base_price,
        10
    )

    high_ask = generator.new_order(
        "S",
        base_price + 2,
        10
    )

    remaining_buy = generator.new_order(
        "B",
        base_price + 1,
        15
    )

    # low_ask was filled.
    # high_ask remains 10.
    # buy remains 5.
    del low_ask

    generator.cancel(remaining_buy)
    generator.cancel(high_ask)

    #
    # Sell can consume 102 but must stop before 100.
    #
    high_bid = generator.new_order(
        "B",
        base_price + 2,
        10
    )

    low_bid = generator.new_order(
        "B",
        base_price,
        10
    )

    remaining_sell = generator.new_order(
        "S",
        base_price + 1,
        15
    )

    # high_bid was filled.
    del high_bid

    generator.cancel(remaining_sell)
    generator.cancel(low_bid)


def scenario_random_sweep(
        generator: CommandGenerator,
        base_price: int,
        depth: int,
        price_span: int,
        rng: random.Random,
) -> None:
    #
    # Random ask book followed by a buy that completely
    # consumes it.
    #
    total_quantity = 0
    highest_price = base_price

    for _ in range(depth):
        price = base_price + rng.randrange(price_span)
        quantity = rng.randint(1, 250)

        generator.new_order(
            "S",
            price,
            quantity
        )

        total_quantity += quantity
        highest_price = max(
            highest_price,
            price
        )

    generator.new_order(
        "B",
        highest_price,
        total_quantity
    )

    #
    # Random bid book followed by a sell that completely
    # consumes it.
    #
    total_quantity = 0
    lowest_price = base_price + price_span - 1

    for _ in range(depth):
        price = base_price + rng.randrange(price_span)
        quantity = rng.randint(1, 250)

        generator.new_order(
            "B",
            price,
            quantity
        )

        total_quantity += quantity
        lowest_price = min(
            lowest_price,
            price
        )

    generator.new_order(
        "S",
        lowest_price,
        total_quantity
    )


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Generate deterministic matching-heavy "
            "Sixchange command streams."
        )
    )

    parser.add_argument(
        "rounds",
        type=int,
        help=(
            "Number of times to repeat the selected "
            "matching scenarios."
        ),
    )

    parser.add_argument(
        "--mode",
        choices=(
            "all",
            "exact",
            "partial",
            "fifo",
            "sweep",
            "limit",
            "random",
        ),
        default="all",
    )

    parser.add_argument(
        "--depth",
        type=int,
        default=32,
        help=(
            "Number of resting orders used in FIFO "
            "and sweep scenarios."
        ),
    )

    parser.add_argument(
        "--price-span",
        type=int,
        default=64,
        help="Price range used by randomized scenarios.",
    )

    parser.add_argument(
        "--base-price",
        type=int,
        default=100,
    )

    parser.add_argument(
        "--symbol",
        default="AAPL",
    )

    parser.add_argument(
        "--seed",
        type=int,
        default=0x51C4A63,
        help="Random seed for reproducible streams.",
    )

    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()

    if arguments.rounds <= 0:
        print(
            "rounds must be positive",
            file=sys.stderr,
        )
        return 1

    if arguments.depth <= 0:
        print(
            "depth must be positive",
            file=sys.stderr,
        )
        return 1

    if arguments.price_span <= 0:
        print(
            "price-span must be positive",
            file=sys.stderr,
        )
        return 1

    rng = random.Random(arguments.seed)

    generator = CommandGenerator(
        arguments.symbol
    )

    scenarios = {
        "exact": lambda: scenario_exact_cross(
            generator,
            arguments.base_price,
            rng,
        ),

        "partial": lambda: scenario_partial_fill(
            generator,
            arguments.base_price,
            rng,
        ),

        "fifo": lambda: scenario_fifo(
            generator,
            arguments.base_price,
            arguments.depth,
            rng,
        ),

        "sweep": lambda: scenario_sweep(
            generator,
            arguments.base_price,
            arguments.depth,
            rng,
        ),

        "limit": lambda: scenario_limit_stop(
            generator,
            arguments.base_price,
        ),

        "random": lambda: scenario_random_sweep(
            generator,
            arguments.base_price,
            arguments.depth,
            arguments.price_span,
            rng,
        ),
    }

    selected_modes = (
        tuple(scenarios.keys())
        if arguments.mode == "all"
        else (arguments.mode,)
    )

    for _ in range(arguments.rounds):
        for mode in selected_modes:
            scenarios[mode]()

    generator.flush()

    print(
        (
            f"generated "
            f"{generator.order_count} orders, "
            f"{generator.cancel_count} cancels, "
            f"{generator.command_count} commands "
            f"(seed={arguments.seed})"
        ),
        file=sys.stderr,
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())