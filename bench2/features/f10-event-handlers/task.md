# Event handlers

Standard input is a sequence of commands, one per line:

- `on EVENT ACTION`: register a handler for `EVENT` (lowercase letters). `ACTION` is one of
  - `say TEXT`: the handler's result is `TEXT`, which is everything after `say ` on the line (it may contain spaces);
  - `echo`: the result is the payload;
  - `upper`: the result is the payload in upper case;
  - `count`: the result is how many times this handler has run so far, including this run (`1`, `2`, ...). Every `count` handler has its own count.
- `emit EVENT PAYLOAD`: run every handler registered for `EVENT`, in registration order, printing `EVENT: RESULT` for each. `PAYLOAD` is everything after `emit EVENT ` (it may contain spaces); for a line `emit EVENT` with nothing after it the payload is empty, and the output is `EVENT: ` followed by the result. If no handler is registered for the event, print `unhandled EVENT`.

Keep the registry as a map from event name to a list of handlers, where each handler is a function that prints its output line. A `count` handler keeps its count in state captured by the function.
