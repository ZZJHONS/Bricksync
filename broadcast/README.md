# BrickSync broadcast message

`message.json` in this folder is the message every running BrickSync shows its user. BrickSync reads it
from `https://raw.githubusercontent.com/ZZJHONS/Bricksync/master/broadcast/message.json` at start and
every 3 hours (when `checkmessage = 1;` in `bricksync.conf.txt`, the default), over HTTPS with the
certificate checked. At the same time it asks the GitHub Releases API for the latest release and tells
the user when a newer one is out. Only someone who can push to `master` can change what users see.

## Format

```json
{
  "timestamp": 1761559200,
  "priority": "high",
  "only_before": "v1.7.4-261027a",
  "message": "Brick Owl now needs SNI.\nPlease update: https://github.com/ZZJHONS/Bricksync/releases/latest"
}
```

| Key | Meaning |
|-----|---------|
| `timestamp` | Unix time (seconds, digits only). A **different** value is a new message; keep it the same when fixing a typo you do not want to re-announce. `date +%s` gives the current one. |
| `priority` | `none`, `low`, `medium`, `high` or `critical`. `none` (or an empty `message`) shows nothing. `low` / `medium`: BrickSync says a message is waiting, the user types `message` to read it. `high` / `critical`: printed in full right away. |
| `only_before` | A release tag. Only builds older than that release see the message (e.g. "update, you are missing the fix"); builds without a release tag (hand builds) compare by their build date. `""` shows it to everyone. |
| `message` | The text, up to 1023 bytes. `\n` for a line break. Control characters (terminal escape codes) are dropped. |

Other keys are ignored. A file that is not valid JSON is ignored too (BrickSync keeps what it had), so
check it before pushing, e.g. `python -m json.tool broadcast/message.json`.

## Posting and withdrawing

1. Edit `message.json`, set a new `timestamp`, commit and push to `master`.
2. Running copies see it within 3 hours (or at once with `message update`).
3. To withdraw it, set `"priority": "none"` and `"message": ""`, with a new `timestamp`.

GitHub may serve the old file for a few minutes after a push (raw.githubusercontent.com caches).
