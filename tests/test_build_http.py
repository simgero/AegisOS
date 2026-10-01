import base64
import hashlib
import importlib.util
import json
import sys
from pathlib import Path
import tempfile
import threading
import time
import unittest
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('build_http', ROOT / 'tools/build-http/server.py')
api = importlib.util.module_from_spec(spec)
spec.loader.exec_module(api)


class HTTPTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.jobs = api.Jobs(self.temp.name)
        self.server = api.Server(('127.0.0.1', 0), self.jobs, hashlib.sha256(b'test-secret').hexdigest())
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.url = 'http://127.0.0.1:' + str(self.server.server_port)
        self.opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))

    def tearDown(self):
        for cancel, done in list(self.jobs.live.values()):
            cancel.set()
            done.wait(5)
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        self.temp.cleanup()

    def request(self, method, path, data=None, auth='Bearer test-secret', **headers):
        headers.update({'Authorization': auth, 'Content-Type': 'application/json'})
        request = urllib.request.Request(self.url + path, data=None if data is None else json.dumps(data).encode(), headers=headers, method=method)
        try: response = self.opener.open(request, timeout=10)
        except urllib.error.HTTPError as exc: response = exc
        with response: return response.status, json.load(response)

    def start(self, command, **kwargs):
        return self.request('POST', '/v1/jobs', dict(command=command, cwd=self.temp.name, wait=5, **kwargs))

    def finish(self, ident):
        for _ in range(100):
            _, data = self.request('GET', '/v1/jobs/' + ident)
            if data['state'] not in ('queued', 'running'): return data
            time.sleep(.05)
        self.fail('Job did not finish')

    def test_auth_and_browser_origin_cannot_execute(self):
        for auth in ('', 'Bearer wrong', 'Basic !!'):
            code, _ = self.request('POST', '/v1/jobs', {'command': 'touch forbidden', 'cwd': self.temp.name}, auth=auth)
            self.assertEqual(code, 401)
        self.assertFalse((Path(self.temp.name) / 'forbidden').exists())
        self.assertEqual(self.request('GET', '/health', Origin='https://example.org')[0], 403)
        basic = 'Basic ' + base64.b64encode(b'codex:test-secret').decode()
        self.assertEqual(self.request('GET', '/health', auth=basic)[0], 200)

    def test_command_cwd_env_stdin_and_exit_code(self):
        code, result = self.start('read -r input; printf "%s:%s:%s" "$MARK" "$input" "$PWD"; echo error >&2; exit 7', env={'MARK': 'ok'}, stdin='from stdin\n')
        self.assertEqual(code, 200)
        self.assertEqual(result['exit_code'], 7)
        self.assertEqual(result['stdout'], 'ok:from stdin:' + str(Path(self.temp.name).resolve()))
        self.assertEqual(result['stderr'], 'error\n')
        self.assertEqual(result['state'], 'finished')
        self.assertFalse(any('from stdin' in p.read_text() for p in Path(self.temp.name).glob('*/job.json')))

    def test_timeout_including_closed_output(self):
        for command in ('sleep 20', 'exec >/dev/null 2>&1; sleep 20'):
            _, data = self.start(command, timeout=.15)
            self.assertEqual(data['state'], 'timed_out')
            self.assertNotEqual(data['exit_code'], 0)

    def test_async_cancel_and_delete(self):
        code, data = self.request('POST', '/v1/jobs', dict(command='echo started; sleep 20', cwd=self.temp.name))
        self.assertEqual(code, 202)
        ident = data['id']
        self.assertEqual(self.request('DELETE', '/v1/jobs/' + ident)[0], 409)
        self.request('POST', '/v1/jobs/' + ident + '/cancel')
        self.assertEqual(self.finish(ident)['state'], 'cancelled')
        self.assertEqual(self.request('DELETE', '/v1/jobs/' + ident)[0], 200)
        self.assertEqual(self.request('GET', '/v1/jobs/' + ident)[0], 404)

    def test_idempotency_and_conflict(self):
        ident = 'a' * 32
        command = 'echo once >> count; cat count'
        _, first = self.start(command, request_id=ident)
        _, second = self.start(command, request_id=ident)
        self.assertEqual(first['stdout'], second['stdout'])
        self.assertEqual((Path(self.temp.name) / 'count').read_text(), 'once\n')
        self.assertEqual(self.start('echo other', request_id=ident)[0], 409)

    def test_output_pagination_and_limit(self):
        _, data = self.start("head -c 100000 /dev/zero")
        self.assertEqual(len(base64.b64decode(data['stdout_base64'])), 65536)
        _, rest = self.request('GET', '/v1/jobs/' + data['id'] + '?stdout_offset=65536')
        self.assertEqual(rest['stdout_next'], 100000)
        old = api.MAX_OUTPUT
        try:
            api.MAX_OUTPUT = 1000
            _, data = self.start('yes output')
            self.assertEqual(data['state'], 'output_limit')
            self.assertEqual(data['stdout_size'], 1000)
        finally: api.MAX_OUTPUT = old

    def test_invalid_requests_and_limits(self):
        for fields in ({'timeout': -1}, {'timeout': float('nan')}, {'env': {'BAD=': 'x'}}, {'request_id': '../bad'}, {'surprise': True}):
            self.assertEqual(self.start('true', **fields)[0], 400)
        self.assertEqual(self.request('POST', '/v1/jobs', [1, 2])[0], 400)
        self.assertEqual(self.request('GET', '/v1/jobs/not-an-id')[0], 404)
        ids = []
        for _ in range(api.MAX_RUNNING):
            _, data = self.request('POST', '/v1/jobs', dict(command='sleep 20', cwd=self.temp.name))
            ids.append(data['id'])
        self.assertEqual(self.start('true')[0], 429)

    def test_restart_preserves_results_marks_unfinished(self):
        _, data = self.start('echo retained')
        restored = api.Jobs(self.temp.name)
        self.assertEqual(restored.result(data['id'])['stdout'], 'retained\n')
        ident = 'b' * 32
        (Path(self.temp.name) / ident).mkdir()
        restored.save(dict(id=ident, state='running', exit_code=None))
        restored = api.Jobs(self.temp.name)
        self.assertEqual(restored.read(ident)['state'], 'interrupted')


if __name__ == '__main__': unittest.main()
