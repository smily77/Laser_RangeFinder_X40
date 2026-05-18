# Parser Test Cases — X40LaserDistanceMeter

These cases document the expected parser output for known raw responses.
Run them mentally or port to a host-side unit test.

## Distance parser (`parseDistance`)

| Raw response  | ok    | meters | millimeters | signalQuality | errorCode | status      |
|---------------|-------|--------|-------------|---------------|-----------|-------------|
| `12.345m,0079`| true  | 12.345 | 12345       | 79            | -1        | OK          |
| `2.345m, 0079`| true  | 2.345  | 2345        | 79            | -1        | OK          |
| ` 2.345m,0079`| true  | 2.345  | 2345        | 79            | -1        | OK (leading space) |
| `0.532m,0123` | true  | 0.532  | 532         | 123           | -1        | OK          |
| `0.010m,0200` | true  | 0.010  | 10          | 200           | -1        | OK          |
| `12.345M,0079`| true  | 12.345 | 12345       | 79            | -1        | OK (uppercase M) |
| `12.345m`     | true  | 12.345 | 12345       | -1            | -1        | OK (no SQ)  |
| `:Er08!`      | false | NAN    | 0           | -1            | 8         | DeviceError (observed format: colon prefix, no dot) |
| `Er.01!`      | false | NAN    | 0           | -1            | 1         | DeviceError (PDF format with dot) |
| `Er.XX!`      | false | NAN    | 0           | -1            | -1        | DeviceError (non-numeric code) |
| `Er.12`       | false | NAN    | 0           | -1            | 12        | DeviceError |
| `D12.345m,0079`| true | 12.345 | 12345       | 79            | -1        | OK (echo prefix 'D' before distance) |
| `` (empty)    | false | NAN    | 0           | -1            | -1        | Timeout     |
| `hello world` | false | NAN    | 0           | -1            | -1        | ParseError  |

## Status parser (`readStatus`)

| Raw response   | temperatureC | supplyV | return |
|----------------|-------------|---------|--------|
| `18.0C, 2.7V`  | 18.0        | 2.7     | true   |
| `18.0'C, 3.0V` | 18.0        | 3.0     | true   |
| `25.5C,3.3V`   | 25.5        | 3.3     | true   |
| `-5.0C, 2.8V`  | -5.0        | 2.8     | true   |
| `` (empty)     | —           | —       | false (Timeout) |
| `garbage`      | —           | —       | false (ParseError) |

## OK parser (`parseOK`)

| Raw response | return | status             |
|--------------|--------|--------------------|
| `OK`         | true   | OK                 |
| `,OK!`       | true   | OK                 |
| `ok`         | true   | OK (case-insensitive) |
| `` (empty)   | false  | Timeout            |
| `Er.01`      | false  | UnexpectedResponse |
| `12.345m,79` | false  | UnexpectedResponse |
