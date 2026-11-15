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
| `D: 1.559m,0613`| true | 1.559 | 1559        | 613           | -1        | OK (observed on device) |
| `?: 1.555m,0634`| true | 1.555 | 1555        | 634           | -1        | OK (garbled echo after laser on, observed) |
| `D:Er08!`     | false | NAN    | 0           | -1            | 8         | DeviceError (observed, no reflection) |
| `M: 6.286m,1923`| true | 6.286 | 6286        | 1923          | -1        | OK (echo 'M' is not taken as unit) |
| `D:33.512m,0001`| true  | 33.512 | 33512      | 1             | -1        | OK (default: no range limit) |
| `D:33.512m,0001`| false | 33.512 | 33512      | 1             | -1        | OutOfRange (only with setMaxRange(30)) |
| `M`           | false | NAN    | 0           | -1            | -1        | ParseError → measurePrecise() reports Unsupported |
| `` (empty)    | false | NAN    | 0           | -1            | -1        | Timeout     |
| `hello world` | false | NAN    | 0           | -1            | -1        | ParseError  |

## Status parser (`readStatus`)

| Raw response   | temperatureC | supplyV | return |
|----------------|-------------|---------|--------|
| `18.0C, 2.7V`  | 18.0        | 2.7     | true   |
| `18.0'C, 3.0V` | 18.0        | 3.0     | true   |
| ``S: 24.8`C,3.3V`` | 24.8    | 3.3     | true (observed on device) |
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
