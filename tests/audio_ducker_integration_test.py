"""Run against the C++ test driver and a fake pactl, no sound server required."""
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import textwrap

def main():
    binary = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        state = root / 'streams.json'
        def write(streams):
            staging = state.with_suffix('.next')
            staging.write_text(json.dumps(streams))
            staging.replace(state)
        def read():
            return json.loads(state.read_text())
        def wait_for(predicate):
            deadline = time.monotonic() + 8
            while time.monotonic() < deadline:
                if predicate(read()):
                    return
                time.sleep(.05)
            raise AssertionError({"streams": read(), "guardian_exit": guardian.poll(), "owner": proc_owner, "cmdline": (Path("/proc") / proc_owner / "cmdline").read_bytes(), "journal": [str(p) for p in root.rglob("audio-ducking*")]})
        pactl = root / 'pactl'
        pactl.write_text(textwrap.dedent('''\
    #!/usr/bin/python3
    import json, os, sys
    from pathlib import Path
    p = Path(os.environ['FAKE_STREAMS'])
    a = sys.argv[1:]
    s = json.loads(p.read_text())
    if a == ['list', 'sink-inputs']:
        for item in s:
            print('Sink Input #' + str(item['id']))
            print('    Volume: ' + ', '.join('channel%d: %d / 50%% / 0 dB' % (i, v) for i, v in enumerate(item['v'])))
            print('        application.name = "' + item['name'] + '"')
            if item.get('key'): print('        module-stream-restore.id = "' + item['key'] + '"')
            print('        object.serial = "' + str(item.get('serial', item['id'])) + '"')
    elif a[0] == 'set-sink-input-volume':
        for item in s:
            if item['id'] == int(a[1]): item['v'] = list(map(int, a[2:]))
        q = p.with_suffix('.command')
        q.write_text(json.dumps(s)); q.replace(p)
    else:
        sys.exit(1)
    '''))
        pactl.chmod(0o755)
        env = {**os.environ, 'PATH': tmp + ':' + os.environ['PATH'],
               'XDG_DATA_HOME': tmp, 'FAKE_STREAMS': str(state), 'FAKE_OWNER_FILE': str(root / 'owner'),
               'COVERPLAYER_AUDIO_DUCKING': '1', 'COVERPLAYER_DUCK_PERCENT': '25'}
        write([{'id': 1, 'name': 'RetroArch', 'v': [32768, 16384]},
               {'id': 2, 'name': 'PipeWire ALSA [coverplayer]', 'v': [65536]}])
        owner = subprocess.Popen(['coverplayer', '--background-audio'], executable=binary, env=env)
        deadline = time.monotonic() + 5
        while not (root / 'owner').exists() and time.monotonic() < deadline:
            time.sleep(.01)
        proc_owner = (root / 'owner').read_text()
        guardian = subprocess.Popen([binary, '--audio-ducking', proc_owner], env=env)
        try:
            wait_for(lambda s: s[0]['v'] == [20643, 10321])
            assert read()[1]['v'] == [65536], 'own stream was ducked'
            s = read(); s.append({'id': 3, 'name': 'EmulationStation', 'v': [48000]}); write(s)
            wait_for(lambda s: s[2]['v'] == [30238])
            time.sleep(.8)
            assert read()[0]['v'] == [20643, 10321], 'repeated attenuation'
            # A manual adjustment must win over automatic restoration.
            s = read(); s[2]['v'] = [9000]; write(s)
            # Reused indices belong to another stream and must not receive old values.
            s = read(); s.append({'id': 4, 'name': 'game', 'v': [40000]}); write(s)
            wait_for(lambda s: s[3]['v'] == [25198])
            s = read(); s[3]['serial'] = 404; s[3]['name'] = 'replacement'; s[3]['v'] = [60000]; write(s)
            wait_for(lambda s: s[3]['v'] == [37798])
            owner.kill(); owner.wait()
            guardian.wait(timeout=8)
            assert guardian.returncode == 0
            assert [s['v'] for s in read()] == [[32768, 16384], [65536], [9000], [60000]]
            # A guardian crash leaves a journal, which a GUI restart can recover.
            (root / 'owner').unlink()
            owner = subprocess.Popen(['coverplayer', '--background-audio'], executable=binary, env=env)
            deadline = time.monotonic() + 5
            while not (root / 'owner').exists() and time.monotonic() < deadline:
                time.sleep(.01)
            proc_owner = (root / 'owner').read_text()
            guardian = subprocess.Popen([binary, '--audio-ducking', proc_owner], env=env)
            wait_for(lambda s: s[0]['v'] == [20643, 10321])
            guardian.kill(); guardian.wait()
            owner.kill(); owner.wait()
            subprocess.run([binary, '--recover'], env=env, check=True)
            assert read()[0]['v'] == [32768, 16384]
            # Capability is disabled outside the Knulli launcher.
            baseline = read()
            subprocess.run([binary, '--audio-ducking', proc_owner],
                           env={**env, 'COVERPLAYER_AUDIO_DUCKING': '0'}, check=False)
            assert read() == baseline
            # The server remembers the previous game's level when RetroArch
            # reconnects. Repeated switches must never multiply the attenuation.
            env.pop('COVERPLAYER_DUCK_PERCENT')  # default = half the amplitude
            (root / 'owner').unlink()
            key = 'sink-input-by-application-name:retroarch'
            write([{'id': 10, 'name': 'retroarch', 'key': key, 'v': [65536, 32768]}])
            owner = subprocess.Popen(['coverplayer', '--background-audio'], executable=binary, env=env)
            deadline = time.monotonic() + 5
            while not (root / 'owner').exists() and time.monotonic() < deadline:
                time.sleep(.01)
            proc_owner = (root / 'owner').read_text()
            guardian = subprocess.Popen([binary, '--audio-ducking', proc_owner], env=env)
            wait_for(lambda s: s[0]['v'] == [52016, 26008])
            for stream_id in range(11, 16):
                write([{'id': stream_id, 'name': 'retroarch', 'key': key, 'v': [52016, 26008]}])
                time.sleep(.8)
                assert read()[0]['v'] == [52016, 26008], 'game switch compounded attenuation'
            owner.kill(); owner.wait()
            guardian.wait(timeout=8)
            assert read()[0]['v'] == [65536, 32768], 'new game did not restore original balance'

            # A game can vanish before playback ends. Keep watching until its
            # remembered volume returns, then restore it even with no audiobook.
            (root / 'owner').unlink()
            owner = subprocess.Popen(['coverplayer', '--background-audio'], executable=binary, env=env)
            deadline = time.monotonic() + 5
            while not (root / 'owner').exists() and time.monotonic() < deadline:
                time.sleep(.01)
            proc_owner = (root / 'owner').read_text()
            guardian = subprocess.Popen([binary, '--audio-ducking', proc_owner], env=env)
            wait_for(lambda s: s[0]['v'] == [52016, 26008])
            write([]); time.sleep(.5)
            owner.kill(); owner.wait()
            time.sleep(.7)
            assert guardian.poll() is None, 'missing stream recovery was abandoned'
            write([{'id': 90, 'name': 'retroarch', 'key': key, 'v': [52016, 26008]}])
            wait_for(lambda s: s[0]['v'] == [65536, 32768])
            guardian.wait(timeout=8)

            # A recovery-only watcher must yield its journal to new playback.
            (root / 'owner').unlink()
            owner = subprocess.Popen(['coverplayer', '--background-audio'], executable=binary, env=env)
            deadline = time.monotonic() + 5
            while not (root / 'owner').exists() and time.monotonic() < deadline:
                time.sleep(.01)
            proc_owner = (root / 'owner').read_text()
            guardian = subprocess.Popen([binary, '--audio-ducking', proc_owner], env=env)
            wait_for(lambda s: s[0]['v'] == [52016, 26008])
            write([]); time.sleep(.5)
            owner.kill(); owner.wait()
            time.sleep(.7)
            previous_guardian = guardian
            (root / 'owner').unlink()
            owner = subprocess.Popen(['coverplayer', '--background-audio'], executable=binary, env=env)
            deadline = time.monotonic() + 5
            while not (root / 'owner').exists() and time.monotonic() < deadline:
                time.sleep(.01)
            proc_owner = (root / 'owner').read_text()
            guardian = subprocess.Popen([binary, '--audio-ducking', proc_owner], env=env)
            try:
                previous_guardian.wait(timeout=5)
            finally:
                if previous_guardian.poll() is None:
                    previous_guardian.kill(); previous_guardian.wait()
            write([{'id': 91, 'name': 'retroarch', 'key': key, 'v': [52016, 26008]}])
            time.sleep(.8)
            assert guardian.poll() is None and read()[0]['v'] == [52016, 26008]
            owner.kill(); owner.wait()
            guardian.wait(timeout=8)
            assert read()[0]['v'] == [65536, 32768]

            print('PASS: late streams, own stream exclusion, stereo balance, no cumulative ducking, manual changes, recycled IDs, SIGKILL restoration, journal recovery, opt-out, five game switches, delayed stream restoration, watcher handoff')
        finally:
            for child in (owner, guardian):
                if child.poll() is None: child.kill()
                child.wait()


if __name__ == "__main__":
    main()
