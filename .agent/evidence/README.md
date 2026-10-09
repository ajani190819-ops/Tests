# Evidence index

Keep large logs, screenshots, exports, and generated diagnostics out of agent
state files. State files should link here or to their original path.

## Current evidence

| Item | Location | Use |
|---|---|---|
| F5W runtime log (latest) | `.agent/evidence/logs/latest.log` | Search for renderer, Iris, Fabric, or Spatial HUD messages. Supplied by the user. |
| F5W runtime log (second session) | `.agent/evidence/logs/l2atest.txt` | Supplied by the user at commit `c8ccc42`. Same build as `latest.log`. |
| Minecraft crash report, 2026-10-08 01:31 | `.agent/evidence/logs/crash-2026-10-08_01.31.10-client.txt` | `IllegalStateException: Texture does not exist` in `MapTextureManager`. Not Spatial HUD. |

Do not delete supplied evidence without the owner's approval. Prefer a dated
filename for future evidence and record the related build commit in the test
report or history note.
