"""Process-level runtime acceptance checks; no broker, hardware, network or models.

Worker gates are FIFOs and are observed through inotify. Timeouts detect hangs;
no arbitrary sleeps determine the order of concurrent operations.
"""
import ctypes
import hashlib
import os
from pathlib import Path
import re
import select
import shutil
import signal
import sqlite3
import subprocess
import sys
import tempfile
import time
import unittest

EXE, SUITE = sys.argv[1:3]
sys.argv[1:] = []


def environment():
    return {k: v for k, v in os.environ.items()
            if not k.startswith(('SMART_HOME_ONTOLOGY_', 'SMART_HOME_RUNTIME_', 'SMART_HOME_LOG_'))}


class Control:
    def __init__(self, path):
        self.path = path
        path.mkdir()
        libc = ctypes.CDLL(None, use_errno=True)
        self.fd = libc.inotify_init1(os.O_CLOEXEC | os.O_NONBLOCK)
        if self.fd < 0 or libc.inotify_add_watch(self.fd, os.fsencode(path), 0x100 | 0x8) < 0:
            raise OSError(ctypes.get_errno(), 'inotify')

    def wait(self, key):
        deadline = time.monotonic() + 15
        while not (self.path / (key + '.reached')).exists():
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not select.select([self.fd], [], [], remaining)[0]:
                raise AssertionError('test point not reached: ' + key)
            os.read(self.fd, 65536)

    def gate(self, key):
        (self.path / (key + '.reached')).unlink(missing_ok=True)
        os.mkfifo(self.path / (key + '.gate'))

    def release(self, key):
        # Called after the corresponding reached event. Open waits for the worker.
        with open(self.path / (key + '.gate'), 'wb', buffering=0) as pipe:
            pipe.write(b'x')
        (self.path / (key + '.gate')).unlink()

    def fail(self, key):
        (self.path / (key + '.fail')).touch()

    def close(self):
        os.close(self.fd)


class App:
    def __init__(self, case, capacity=4, ready=True):
        env = environment()
        env['SMART_HOME_RUNTIME_TEST_CONTROL'] = str(case.control.path)
        self.process = subprocess.Popen(
            [EXE, '--runtime-state-dir', str(case.state), '--runtime-ram-dir', str(case.ram),
             '--runtime-snapshot-capacity', str(capacity), '--log-level=off', '--log-file='],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
        case.addCleanup(self.kill)
        os.set_blocking(self.process.stdout.fileno(), False)
        self.output = ''
        self.cursor = 0
        if ready:
            self.expect('Scan: 3 processed, 0 errors')

    def send(self, text):
        self.process.stdin.write(text.encode())
        self.process.stdin.flush()

    def expect(self, pattern):
        deadline = time.monotonic() + 15
        while True:
            match = re.search(pattern, self.output[self.cursor:])
            if match:
                self.cursor += match.end()
                return match
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not select.select([self.process.stdout], [], [], remaining)[0]:
                raise AssertionError(f'timed out waiting for {pattern!r}: {self.output}')
            data = os.read(self.process.stdout.fileno(), 65536)
            if not data:
                raise AssertionError(f'process exited waiting for {pattern!r}: {self.output}\n'
                                     + self.process.stderr.read().decode())
            self.output += data.decode()

    def initial_save(self):
        self.expect('Saved revision 2')

    def change(self, text):
        self.send(text + '\n')
        revision = int(self.expect(r'Changes applied; save pending revision (\d+)')[1])
        self.expect(f'Saved revision {revision}')
        return revision

    def finish(self, command='exit\n', code=0):
        if command is None:
            self.process.stdin.close()
        elif isinstance(command, int):
            self.process.send_signal(command)
        else:
            self.send(command)
        self.process.wait(timeout=20)
        self.output += self.process.stdout.read().decode()
        errors = self.process.stderr.read().decode()
        if self.process.returncode != code:
            raise AssertionError(f'exit {self.process.returncode}, expected {code}: {self.output}\n{errors}')
        self.process.stdout.close()
        self.process.stderr.close()
        if not self.process.stdin.closed:
            self.process.stdin.close()
        return errors

    def kill(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=10)
        for pipe in (self.process.stdin, self.process.stdout, self.process.stderr):
            if not pipe.closed:
                pipe.close()


class RuntimeCase(unittest.TestCase):
    def setUp(self):
        self.disk = tempfile.TemporaryDirectory(prefix='runtime-tests-')
        self.addCleanup(self.disk.cleanup)
        self.state = Path(self.disk.name) / 'state'
        self.ram_temp = tempfile.TemporaryDirectory(prefix='runtime-tests-', dir='/dev/shm')
        self.addCleanup(self.ram_temp.cleanup)
        self.ram = Path(self.ram_temp.name)
        self.control = Control(Path(self.disk.name) / 'control')
        self.addCleanup(self.control.close)

    def generation(self):
        selected = (self.state / 'CURRENT').read_text().strip()
        path = self.state / 'generations' / selected
        meta = (path / 'metadata').read_text().splitlines()
        self.assertEqual(meta[0], 'SMART_HOME_RUNTIME 1')
        for name, expected in zip(('state.sqlite', 'ontology.lbdb'), meta[2:]):
            self.assertEqual(hashlib.sha256((path / name).read_bytes()).hexdigest(), expected)
        with sqlite3.connect(path / 'state.sqlite') as db:
            self.assertEqual(db.execute('select revision from runtime_meta').fetchone()[0], int(meta[1]))
            records = db.execute('select local_id,physical_id,driver_id,name from devices order by local_id').fetchall()
            values = db.execute('select * from runtime_values order by id').fetchall()
            self.assertEqual(len(records), len(values))
        return path, records, values

    def cli(self, *arguments):
        return subprocess.run([EXE, '--runtime-state-dir', str(self.state),
                               '--runtime-ram-dir', str(self.ram), '--log-level=off', '--log-file=',
                               *arguments], input='', capture_output=True, text=True, env=environment(), timeout=20)


class Console(RuntimeCase):
    def test_commands_and_identity(self):
        app = App(self)
        app.initial_save()
        app.send('list\nshow 1\nshow 3\n')
        app.expect('Device 1 Light1 light room=living_room')
        app.expect('Device 2 Light2 light room=living_room')
        app.expect('Device 3 TemperatureSensor in bath temperature_sensor room=bathroom')
        app.expect('brightness = 50')
        app.expect('color = 255 255 255')
        app.expect('on_off = false')
        app.expect('temperature = 23')
        app.change('1 brightness.set 75')
        app.change('living_room light on_off.set true')
        app.change('living_room light color.set 10 20 30')
        count = len(re.findall('Save queued revision', app.output))
        app.send('scan\n1 brightness.set 75\n1 brightness.get\nbathroom temperature_sensor temperature.get\n')
        app.expect('Scan: 3 processed, 0 errors')
        app.expect('Device 1 brightness.set: applied')
        app.expect('Device 1 brightness.get: 75')
        app.expect('Device 3 temperature.get: 23')
        self.assertEqual(len(re.findall('Save queued revision', app.output)), count)
        app.finish()
        _, before_records, before_values = self.generation()
        again = App(self)
        again.send('show 1\nshow 2\nshow 3\n')
        again.expect('brightness = 75')
        again.expect('color = 10 20 30')
        again.expect('on_off = true')
        again.expect('brightness = 50')
        again.expect('color = 10 20 30')
        again.expect('on_off = true')
        again.expect('temperature = 23')
        again.finish()
        _, records, values = self.generation()
        self.assertEqual(before_records, records)
        self.assertEqual(before_values, values)
        self.assertFalse(any(self.ram.iterdir()))

    def test_parser_and_partial_group(self):
        app = App(self)
        app.initial_save()
        for text in ('-1 brightness.get', '9223372036854775808 brightness.get', '1x brightness.get',
                     '1', '1 bogus', '1 brightness.get extra', '1 brightness.set 2x',
                     '1 on_off.set 1', '1 color.set 256 0 0', '1 color.set -1 0 0',
                     '1 color.set 0 0', 'living_room fan on_off.get', 'absent light on_off.get',
                     'show -1', 'show 999', 'help extra'):
            app.send(text + '\n')
            app.expect('Error:')
        app.send('1 brightness.set 101\n3 color.set 1 2 3\n999 on_off.get\n')
        app.expect('Device 1 error:')
        app.expect('Device 3 error:')
        app.expect('Device 999 error:')
        self.control.fail('target-2-2')
        app.change('living_room light brightness.set 85')
        self.assertIn('Device 2 error:', app.output)
        (self.control.path / 'target-2-2.fail').unlink()
        app.send('1 brightness.get\n2 brightness.get\n')
        app.expect('Device 1 brightness.get: 85')
        app.expect('Device 2 brightness.get: 50')
        app.finish()
        _, _, values = self.generation()
        self.assertEqual([values[0][3], values[1][3]], [85, 50])

    def test_eof_tail_and_long_line(self):
        app = App(self)
        app.initial_save()
        app.send('x' * 70000 + '\nhelp\n')
        app.expect('Error: input line too long.')
        app.expect('Operations:')
        app.send('1 brightness.set 61')
        app.finish(None)
        _, _, values = self.generation()
        self.assertEqual(values[0][3], 61)


class Persistence(RuntimeCase):
    def test_crash_at_each_commit_boundary(self):
        app = App(self)
        app.initial_save()
        app.finish()
        baseline = Path(self.disk.name) / 'baseline'
        shutil.copytree(self.state, baseline)
        for point in ('before-files', 'state.sqlite', 'ontology.lbdb', 'metadata', 'generation',
                      'before-current', 'after-current', 'committed'):
            with self.subTest(point=point):
                shutil.rmtree(self.state)
                shutil.copytree(baseline, self.state)
                key = point + '-3'
                self.control.gate(key)
                app = App(self)
                app.send('living_room light brightness.set 77\n')
                self.control.wait(key)
                app.kill()  # SIGKILL: no cleanup/final save.
                (self.control.path / (key + '.gate')).unlink()
                _, records, values = self.generation()
                expected = 77 if point in ('after-current', 'committed') else 50
                self.assertEqual([v[3] for v in values[:2]], [expected, expected])
                self.assertEqual([r[0] for r in records], [1, 2, 3])
                again = App(self)
                again.send('show 1\nshow 2\n')
                again.expect(f'brightness = {expected}')
                again.expect(f'brightness = {expected}')
                again.finish()
                (self.control.path / (key + '.reached')).unlink()

    def test_corruption_never_resets(self):
        app = App(self)
        app.initial_save()
        app.finish()
        baseline = Path(self.disk.name) / 'baseline'
        shutil.copytree(self.state, baseline)
        for corruption in ('current', 'missing', 'version', 'graph', 'revision', 'values', 'driver'):
            with self.subTest(corruption=corruption):
                shutil.rmtree(self.state)
                shutil.copytree(baseline, self.state)
                path = self.state / 'generations' / (self.state / 'CURRENT').read_text().strip()
                if corruption == 'current':
                    (self.state / 'CURRENT').write_text('../escape\n')
                elif corruption == 'missing':
                    (path / 'ontology.lbdb').unlink()
                elif corruption == 'version':
                    (path / 'metadata').write_text('SMART_HOME_RUNTIME 99\n')
                elif corruption == 'graph':
                    with open(path / 'ontology.lbdb', 'r+b') as file:
                        file.write(b'corrupt')
                else:
                    with sqlite3.connect(path / 'state.sqlite') as db:
                        db.execute({'revision': 'update runtime_meta set revision=999',
                                    'values': 'update runtime_values set brightness=200 where id=1',
                                    'driver': "update devices set driver_id='unsupported' where local_id=1"}[corruption])
                    meta = (path / 'metadata').read_text().splitlines()
                    meta[2] = hashlib.sha256((path / 'state.sqlite').read_bytes()).hexdigest()
                    (path / 'metadata').write_text('\n'.join(meta) + '\n')
                result = self.cli()
                self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
                self.assertNotIn('Runtime ready', result.stdout)
                self.assertIn('Runtime failure:', result.stderr)


class Events(RuntimeCase):
    def test_partial_input_worker_events_and_backpressure(self):
        self.control.gate('before-files-2')
        app = App(self, capacity=1)
        self.control.wait('before-files-2')
        app.send('1 brightness.set 70\nlist\n1 brightness.')
        app.expect('Snapshot queue full; change rejected before execution')
        app.expect('Device 3 TemperatureSensor in bath')
        self.control.release('before-files-2')
        app.expect('Saved revision 2')  # Completing a save does not need a newline on stdin.
        app.send('get\n')
        app.expect('Device 1 brightness.get: 50')
        app.finish()

    def test_old_ack_does_not_confirm_new_revision(self):
        app = App(self)
        app.initial_save()
        self.control.gate('before-files-3')
        self.control.gate('before-files-4')
        app.send('1 brightness.set 71\n')
        app.expect('Save queued revision 3')
        self.control.wait('before-files-3')
        app.send('1 brightness.set 72\n')
        app.expect('Save queued revision 4')
        self.control.release('before-files-3')
        app.expect('Saved revision 3')
        self.control.wait('before-files-4')
        app.send('1 brightness.get\n')
        app.expect('Device 1 brightness.get: 72')
        self.assertNotIn('Saved revision 4', app.output)
        self.control.release('before-files-4')
        app.expect('Saved revision 4')
        app.finish()
        self.assertEqual(self.generation()[2][0][3], 72)

    def test_shutdown_waits_for_queue_and_signals(self):
        for signum in (signal.SIGINT, signal.SIGTERM):
            with self.subTest(signal=signum):
                if self.state.exists():
                    shutil.rmtree(self.state)
                key = 'before-files-2'
                self.control.gate(key)
                app = App(self, capacity=1)
                self.control.wait(key)
                app.send('1 brightness.')  # Incomplete input cannot mask a signal.
                app.process.send_signal(signum)
                app.expect('Stopping; final save pending.')
                self.assertIsNone(app.process.poll())
                self.control.release(key)
                app.process.wait(timeout=20)
                self.assertEqual(app.process.returncode, 0)
                self.generation()
                app.kill()
                (self.control.path / (key + '.reached')).unlink()

    def test_single_writer(self):
        app = App(self)
        app.initial_save()
        second = self.cli()
        self.assertEqual(second.returncode, 1)
        self.assertIn('locked by another process', second.stderr)
        app.finish()


class Failures(RuntimeCase):
    def test_retry_preparation_and_worker_failures(self):
        app = App(self)
        app.initial_save()
        baseline = self.generation()[0].name
        for point, brightness, revision in (('prepare', 73, 3), ('before-current', 74, 4), ('after-current', 75, 5)):
            with self.subTest(point=point):
                key = f'{point}-{revision}'
                self.control.fail(key)
                app.send(f'1 brightness.set {brightness}\n')
                app.expect(f'Changes applied; save pending revision {revision}')
                app.expect('Save preparation failed' if point == 'prepare' else f'Save failed for revision {revision}')
                if point != 'after-current':
                    self.assertEqual(self.generation()[0].name, baseline)
                app.send('1 brightness.get\n')
                app.expect(f'Device 1 brightness.get: {brightness}')
                (self.control.path / (key + '.fail')).unlink()
                app.send('save\n')
                app.expect(f'Saved revision {revision}')
                baseline = self.generation()[0].name
        app.finish()

    def test_final_failure_even_after_same_revision_was_saved(self):
        app = App(self)
        app.initial_save()
        self.control.fail('before-current-2')
        errors = app.finish(code=1)
        self.assertIn('recoverable RAM files retained', errors)
        self.assertTrue(any(self.ram.iterdir()))
        (self.control.path / 'before-current-2.fail').unlink()
        again = App(self)
        again.finish()
        self.generation()

    def test_final_preparation_failure(self):
        app = App(self)
        app.initial_save()
        self.control.fail('prepare-2')
        app.finish(code=1)
        self.assertTrue(any(self.ram.iterdir()))

    def test_configuration_and_no_disk_fallback(self):
        for arguments in (('--runtime-state-dir=',), ('--runtime-snapshot-capacity=0',),
                          ('--runtime-snapshot-capacity=-1',), ('--runtime-snapshot-capacity=2x',),
                          ('--runtime-unknown=1',), ('--ontology-sync-seconds=1',),
                          ('--ontology-path=/tmp/independent.lbdb',), ('--log-level=invalid',)):
            with self.subTest(arguments=arguments):
                result = self.cli(*arguments)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, '')
        result = self.cli('--runtime-ram-dir=/proc')
        self.assertEqual(result.returncode, 1)
        self.assertIn('tmpfs/ramfs', result.stderr)
        result = self.cli('--runtime-ram-dir=' + self.disk.name + '/missing')
        self.assertEqual(result.returncode, 1)


if __name__ == '__main__':
    suites = {'console': Console, 'persistence': Persistence, 'events': Events, 'failures': Failures}
    unittest.main(defaultTest=suites[SUITE].__name__, verbosity=2)
