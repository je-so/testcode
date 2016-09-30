### Schaltplan des TM4C123G

Die Pins 0 und 4 des PortF sind vorbeschaltet mit den Tastern SW1 und SW2.
Die Pins 1 bis 3 sind mit der RGB LED auf dem Board verbunden.


```
        ┌──────────────────┐
        │ TM4C123          │   LED grün    ┬ 5V
        │                  │ ╭─|◀──⌁───────┤
        │         (out)PF3 ├⌁◯             │
 ╭──────┤ PF4(in)          │ ⍊   LED blau  │
 /SW1   │                  │   ╭─|◀──⌁─────┤
 │ ╭────┤ PF0(in) (out)PF2 ├──⌁◯           │
 │ /SW2 │                  │   ⍊   LED rot │
 │ │    │                  │     ╭─|◀──⌁───┘
 ⍊ ⍊    │         (out)PF1 ├────⌁◯
 GND    └──────────────────┘     ⍊
```
> ◯ Symbol bezeichnet Transistor.
Die LEDs sind als eine einzige RGB LED auf dem Board zusammengefasst.
