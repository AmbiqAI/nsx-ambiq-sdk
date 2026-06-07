# nsx-timer

`nsx-timer` provides caller-directed timer, ticker, and periodic callback helpers for AmbiqSuite-backed NSX targets.

It owns hardware timer setup and ISR callback routing. Application scheduling, task loops, and semantic timer roles remain with the caller or board/application layer.

Public interfaces live in `includes-api/`.
