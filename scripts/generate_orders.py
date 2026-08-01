#!/usr/bin/env python3

import argparse
import sys


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate deterministic Sixchange commands."
    )

    parser.add_argument(
        "count",
        type=int,
        help="Number of unique client orders to generate.",
    )

    parser.add_argument(
        "--mode",
        choices=("add", "churn"),
        default="churn",
        help=(
            "add: leave every order active; "
            "churn: add and immediately cancel every order"
        ),
    )

    parser.add_argument(
        "--price-levels",
        type=int,
        default=1_000,
        help="Number of prices to cycle through.",
    )

    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()

    if arguments.count < 1:
        print("count must be positive", file=sys.stderr)
        return 1

    if arguments.price_levels < 1:
        print("price-levels must be positive", file=sys.stderr)
        return 1

    output = sys.stdout
    buffer: list[str] = []

    for client_order_id in range(1, arguments.count + 1):
        side = "B" if client_order_id % 2 else "S"
        price = 100 + client_order_id % arguments.price_levels
        quantity = 1 + client_order_id % 100

        buffer.append(
            f"N {client_order_id} AAPL "
            f"{side} L GFD {price} {quantity}\n"
        )

        if arguments.mode == "churn":
            buffer.append(
                f"C {client_order_id} AAPL\n"
            )

        # Write in batches so Python does not flush for every command.
        if len(buffer) >= 8_192:
            output.write("".join(buffer))
            buffer.clear()

    if buffer:
        output.write("".join(buffer))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())