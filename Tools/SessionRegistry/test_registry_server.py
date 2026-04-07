import json
import threading
import time
import unittest
from urllib.error import HTTPError
from urllib.request import Request, urlopen

from registry_server import build_server


class RegistryServerApiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.token = "test-token"
        cls.server, cls.stop_event, cls.cleanup_thread = build_server(
            host="127.0.0.1",
            port=0,
            token=cls.token,
            ttl_seconds=60,
            cleanup_interval=1,
        )
        cls.thread = threading.Thread(target=cls.server.serve_forever, daemon=True)
        cls.thread.start()
        host, port = cls.server.server_address
        cls.base_url = f"http://{host}:{port}"

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.stop_event.set()
        cls.cleanup_thread.join(timeout=1.0)
        cls.server.server_close()

    def _request(self, method: str, path: str, data=None, auth=True):
        body = None
        headers = {"Accept": "application/json"}
        if data is not None:
            body = json.dumps(data).encode("utf-8")
            headers["Content-Type"] = "application/json"
        if auth:
            headers["Authorization"] = f"Bearer {self.token}"

        request = Request(
            url=f"{self.base_url}{path}",
            data=body,
            headers=headers,
            method=method,
        )
        with urlopen(request, timeout=3) as response:
            payload = json.loads(response.read().decode("utf-8"))
            return response.getcode(), payload

    def _request_text(self, method: str, path: str, auth=True):
        headers = {}
        if auth:
            headers["Authorization"] = f"Bearer {self.token}"
        request = Request(url=f"{self.base_url}{path}", headers=headers, method=method)
        with urlopen(request, timeout=3) as response:
            body = response.read().decode("utf-8")
            return response.getcode(), response.headers.get("Content-Type", ""), body

    def test_auth_is_required_for_sessions(self):
        with self.assertRaises(HTTPError) as ctx:
            self._request("GET", "/sessions", auth=False)
        self.assertEqual(ctx.exception.code, 401)
        ctx.exception.close()

    def test_create_list_heartbeat_delete_flow(self):
        status, created = self._request(
            "POST",
            "/sessions",
            data={
                "serverName": "Lab Session",
                "ownerName": "Server PC",
                "connectAddress": "10.0.0.25",
                "connectPort": 7777,
                "maxPlayers": 16,
                "buildUniqueId": 1,
            },
        )
        self.assertEqual(status, 201)
        self.assertIn("sessionId", created)
        self.assertEqual(created["connectString"], "10.0.0.25:7777")

        session_id = created["sessionId"]

        status, listing = self._request("GET", "/sessions")
        self.assertEqual(status, 200)
        self.assertGreaterEqual(len(listing["sessions"]), 1)

        status, heartbeat = self._request("POST", f"/sessions/{session_id}/heartbeat", data={})
        self.assertEqual(status, 200)
        self.assertEqual(heartbeat["status"], "heartbeat_updated")

        status, deleted = self._request("DELETE", f"/sessions/{session_id}")
        self.assertEqual(status, 200)
        self.assertEqual(deleted["status"], "deleted")

    def test_admin_page_is_served(self):
        status, content_type, body = self._request_text("GET", "/admin", auth=False)
        self.assertEqual(status, 200)
        self.assertIn("text/html", content_type)
        self.assertIn("OpenXrMp Dedicated Session Admin", body)

    def test_admin_sessions_contains_rich_metadata(self):
        status, created = self._request(
            "POST",
            "/sessions",
            data={
                "serverName": "Admin Meta Session",
                "connectAddress": "127.0.0.1",
                "connectPort": 7777,
            },
        )
        self.assertEqual(status, 201)

        status, admin_listing = self._request("GET", "/admin/sessions")
        self.assertEqual(status, 200)
        self.assertIn("sessions", admin_listing)
        self.assertIn("ttlSeconds", admin_listing)
        self.assertIn("generatedAt", admin_listing)

        match = None
        for row in admin_listing["sessions"]:
            if row.get("sessionId") == created["sessionId"]:
                match = row
                break

        self.assertIsNotNone(match)
        self.assertIn("createdAt", match)
        self.assertIn("lastHeartbeatAt", match)
        self.assertIn("staleAgeSeconds", match)
        self.assertIn("isStale", match)
        self.assertIsInstance(match["staleAgeSeconds"], (int, float))
        self.assertGreaterEqual(match["staleAgeSeconds"], 0)

    def test_ttl_cleanup_removes_stale_sessions(self):
        short_lived_server, stop_event, cleanup_thread = build_server(
            host="127.0.0.1",
            port=0,
            token=self.token,
            ttl_seconds=1,
            cleanup_interval=1,
        )
        thread = threading.Thread(target=short_lived_server.serve_forever, daemon=True)
        thread.start()

        host, port = short_lived_server.server_address
        base_url = f"http://{host}:{port}"
        headers = {
            "Authorization": f"Bearer {self.token}",
            "Accept": "application/json",
            "Content-Type": "application/json",
        }

        create_req = Request(
            url=f"{base_url}/sessions",
            data=json.dumps({"connectString": "127.0.0.1:7777"}).encode("utf-8"),
            headers=headers,
            method="POST",
        )
        with urlopen(create_req, timeout=3):
            pass

        time.sleep(2.2)

        list_req = Request(url=f"{base_url}/sessions", headers=headers, method="GET")
        with urlopen(list_req, timeout=3) as response:
            listing = json.loads(response.read().decode("utf-8"))
            self.assertEqual(listing["sessions"], [])

        short_lived_server.shutdown()
        stop_event.set()
        cleanup_thread.join(timeout=1.0)
        short_lived_server.server_close()


if __name__ == "__main__":
    unittest.main()


