# checkpoint.hpp {#rec_checkpoint}

The checkpoint says how far a task got (model/checkpoint.hpp). Every device taking part in the process keeps and sends the freshest copy it knows, so a successor picks up from there instead of starting over.

| Field | |
|---|---|
| `epoch` | the epoch of the holder that wrote it |
| `seq` | the number of the snapshot within that epoch |
| `progress` | the application's state (the library never looks inside) |

Copies are compared by epoch first and by `seq` after. Always keeping the fresher one gives the same result whatever order messages arrive in, and a snapshot from an old holder can never overwrite the successor's. `seq` goes up by one every time the holder's work changes the state, and goes back to zero when a new holder takes over with a new epoch. In messages a checkpoint takes 4 bytes plus the application's state.
