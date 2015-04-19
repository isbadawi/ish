# ish

It's a shell.
So far it just supports spawning commands and piping them together, e.g.

```bash
$ ish
ish$ echo hello | tr [:lower:] [:upper:] | xargs yes | head -5
HELLO
HELLO
HELLO
HELLO
HELLO
ish$
```
