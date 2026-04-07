# OpenXrMp Session Registry (Python)

Small on-prem registry API for dedicated server discovery.

## What it does

- `POST /sessions` - register/create a session row.
- `GET /sessions` - return discoverable sessions for Unreal clients.
- `POST /sessions/{sessionId}/heartbeat` - keep session alive.
- `DELETE /sessions/{sessionId}` - remove session row.
- `GET /health` - health endpoint.
- `GET /admin` - simple HTML admin panel.
- `GET /admin/sessions` - rich admin session view (`createdAt`, `lastHeartbeatAt`, `staleAgeSeconds`, `isStale`).

This matches the dedicated flow in `XrMpGameInstance` where:

- Host calls `POST /sessions`
- Client browser calls `GET /sessions`
- Join uses returned `connectString` (or derived `address:port`)

## Run

```powershell
python "C:\Users\ZeroR\Documents\Unreal Projects\OpenXrMp\Plugins\OpenXrMultiplayer\Tools\SessionRegistry\registry_server.py" --host 0.0.0.0 --port 8080 --token change-me
```

Environment variable equivalents:

- `SESSION_REGISTRY_HOST`
- `SESSION_REGISTRY_PORT`
- `SESSION_REGISTRY_TOKEN`
- `SESSION_REGISTRY_TTL_SECONDS`
- `SESSION_REGISTRY_CLEANUP_INTERVAL`

## Unreal setup

In your `BP_Gameinstance` (derived from `XrMpGameInstance`):

- `SetNetworkMode(Dedicated)`
- Set:
  - `DedicatedApiBaseUrl` -> `http://<server-ip>:8080`
  - `DedicatedApiListRoute` -> `/sessions`
  - `DedicatedApiCreateRoute` -> `/sessions`
  - `DedicatedApiToken` -> same token as server

## API examples

Create/register:

```json
{
  "serverName": "Teacher Session",
  "ownerName": "On-Prem Server",
  "connectAddress": "10.0.0.25",
  "connectPort": 7777,
  "maxPlayers": 16,
  "currentPlayers": 0,
  "buildUniqueId": 1,
  "mode": "dedicated",
  "map": "/Game/VRTemplate/VRTemplateMap"
}
```

Admin list response (richer metadata):

```json
{
  "sessions": [
    {
      "sessionId": "...",
      "serverName": "Teacher Session",
      "connectString": "10.0.0.25:7777",
      "createdAt": "2026-04-07T16:45:00+00:00",
      "lastHeartbeatAt": "2026-04-07T16:45:09+00:00",
      "staleAgeSeconds": 9.13,
      "isStale": false
    }
  ],
  "ttlSeconds": 120,
  "generatedAt": "2026-04-07T16:45:09+00:00"
}
```

## Heartbeat Client (Create -> Heartbeat -> Delete)

Use `heartbeat_client.py` on the dedicated server host so session rows stay fresh and are cleaned up on shutdown.

```powershell
python "C:\Users\ZeroR\Documents\Unreal Projects\OpenXrMp\Plugins\OpenXrMultiplayer\Tools\SessionRegistry\heartbeat_client.py" \
  --base-url http://127.0.0.1:8080 \
  --token change-me \
  --connect-address 10.0.0.25 \
  --connect-port 7777 \
  --server-name "Teacher Session" \
  --heartbeat-interval 10
```

Open the admin panel in browser:

```text
http://<server-ip>:8080/admin
```

Response includes:

```json
{
  "sessionId": "...",
  "serverName": "Teacher Session",
  "connectString": "10.0.0.25:7777",
  "maxPlayers": 16,
  "currentPlayers": 0
}
```

List response:

```json
{
  "sessions": [
    {
      "sessionId": "...",
      "serverName": "Teacher Session",
      "ownerName": "On-Prem Server",
      "connectString": "10.0.0.25:7777",
      "maxPlayers": 16,
      "currentPlayers": 0,
      "pingMs": -1,
      "buildUniqueId": 1,
      "mode": "dedicated",
      "map": "/Game/VRTemplate/VRTemplateMap"
    }
  ]
}
```

## Test

```powershell
Push-Location "C:\Users\ZeroR\Documents\Unreal Projects\OpenXrMp\Plugins\OpenXrMultiplayer\Tools\SessionRegistry"
python -m unittest -v test_registry_server.py
Pop-Location
```

## Linux server notes

On Ubuntu, open the required ports:

- TCP `8080` for this registry API
- UDP `7777` for Unreal dedicated game traffic

You can run this registry as a `systemd` service and keep Unreal dedicated server as a separate service/process.

