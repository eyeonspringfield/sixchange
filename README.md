# Sixchange

Sixchange (Simulated Exchange) is an exchange matching engine with a large set of (planned) features.

## Text codec

TextCodec converts between the external text protocol and internal typed messages.

A new order uses the following format:
```
N <client-order-id> <symbol> <side> <order-type> <time-in-force> <price> <quantity>
```

Example:

```
N 1001 AAPL B L GFD 100 50
```