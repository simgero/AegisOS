#!/usr/bin/env python3
"""Interactive synthetic two-user test for an explicitly selected fresh local QEMU.

Build Android on aegis-build; use checked local images or verified release images.
This host-side tool only drives the running local guest. It creates synthetic
AOSP test users when commanded; passwords remain in this process's memory.
Do not use a personal profile. Existing users or output directories are refused.
It never boots, shuts down, resets, promotes or deletes a profile.
"""
import argparse, hashlib, json, os, pty, re, secrets, select, shlex, signal, subprocess, sys, time, fcntl, struct, termios, uuid
import xml.etree.ElementTree as ET
from pathlib import Path

def checked_output(*args, **kwargs):
    # Diagnostic ADB commands must never consume the driver's control input.
    kwargs.setdefault('stdin', subprocess.DEVNULL)
    return subprocess.check_output(*args, **kwargs)


def run_control(*args, **kwargs):
    if 'input' not in kwargs:
        kwargs.setdefault('stdin', subprocess.DEVNULL)
    return subprocess.run(*args, **kwargs)


parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--run',type=Path,required=True,help='Existing local QEMU run directory with profile-path.txt and adb-address.txt')
parser.add_argument('--prepared',type=Path,required=True,help='Prepared image directory with avb-checked.json')
parser.add_argument('--commit',required=True,help='Exact full image builder commit')
parser.add_argument('--output',type=Path,required=True,help='New evidence directory; existing directories are refused')
args=parser.parse_args()
if sys.platform not in ('darwin', 'linux'):parser.error('Guest tests require macOS or Linux')
if sys.flags.optimize:parser.error('Run without Python -O; acceptance assertions must stay enabled')
if not re.fullmatch(r'[0-9a-f]{40}',args.commit):parser.error('Expected a full hexadecimal commit')
RUN=args.run.resolve(strict=True)
PREPARED=args.prepared.resolve(strict=True)
COMMIT=args.commit
OUT=args.output.absolute()
if OUT.exists():parser.error('Output already exists; preserve the prior evidence and driver')
PROFILE=Path((RUN/'profile-path.txt').read_text().strip()).resolve(strict=True)
manifest=json.loads((PROFILE/'profile.json').read_text())
PROFILE_ID=manifest['profile_id']
assert str(uuid.UUID(PROFILE_ID))==PROFILE_ID and manifest['version']==1
assert Path(manifest['bindings']['base_disk']['path']).resolve()==PREPARED/'android.raw'
assert Path(manifest['bindings']['bootconfig']['path']).resolve()==PREPARED/'runtime.bootconfig'
address=(RUN/'adb-address.txt').read_text().strip()
assert re.fullmatch(r'127\.0\.0\.1:[0-9]{4,5}',address) and 1024<=int(address.rsplit(':',1)[1])<=65535
ADB=['adb','-s',address]
receipt=json.loads((PREPARED/'avb-checked.json').read_text())
assert receipt['builder_commit']==COMMIT
assert receipt['status'] in ('TEST_KEY_AVB_CHECKED_DISK_PREPARED_NOT_BOOTED',
                             'TEST_KEY_AVB_CHECKED_PARTITIONS_PREPARED_NOT_BOOTED')
disk_receipt=json.loads((PREPARED/'android.raw.json').read_text())
assert disk_receipt['sha256']==manifest['bindings']['base_disk']['sha256']
assert checked_output(ADB+['shell','getprop','ro.boot.vbmeta.digest'],text=True,timeout=20).strip()==receipt['vbmeta_digest']
assert checked_output(ADB+['shell','getprop','sys.boot_completed'],timeout=20).strip()==b'1'
assert checked_output(ADB+['shell','getenforce'],timeout=20).strip()==b'Enforcing'
assert checked_output(ADB+['shell','getprop','ro.adb.secure'],timeout=20).strip()==b'1'
assert checked_output(ADB+['shell','getprop','ro.aegis.runtime.mode'],timeout=20).strip()==b'managed-v1'
assert re.findall(r'UserInfo\{([0-9]+):',checked_output(ADB+['shell','pm','list','users'],text=True,timeout=20))==['0'], 'Refuse an existing personal profile'
assert set(re.findall(r'UserInfo\{([0-9]+):',checked_output(ADB+['shell','dumpsys','user'],text=True,timeout=20)))=={'0'}, 'Refuse partial or removing personal users'
assert 'CE unlocked users: [0]' in checked_output(ADB+['shell','dumpsys','mount'],text=True,timeout=20).splitlines()
assert 'populated 0' in checked_output(ADB+['shell','su','0','cat','/sys/fs/cgroup/aegis-runtime/contexts/cgroup.events'],text=True,timeout=20).splitlines()
OUT.mkdir(parents=True,exist_ok=False)
(OUT/'expected-profile-id.txt').write_text(PROFILE_ID+'\n')
(OUT/'inputs.json').write_text(json.dumps({'run':str(RUN),'prepared':str(PREPARED),'profile':str(PROFILE),'profile_id':PROFILE_ID,'image_commit':COMMIT,'adb_address':address,'driver_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'initial_users':[0],'limits':['Host orchestration only; no image compilation or profile promotion','Complete credentials are generated in memory; process exit prevents later reuse of these synthetic accounts']},indent=2)+'\n')
credentials = {name: bytearray(secrets.choice(b'abcdefghjkmnpqrstuvwxyz23456789')
                              for _ in range(16)) for name in ('alpha', 'beta', 'newbeta', 'wrong')}
names = {'alpha': 'Runtime Test Alpha', 'beta': 'Runtime Test Beta'}
current_credentials = {'alpha':'alpha', 'beta':'beta'}
probes = {name: secrets.token_bytes(4096) for name in names}
gnu_probes = {name: secrets.token_hex(512) for name in names}
gnu_files = {name: 'proof-'+secrets.token_hex(12) for name in names}
gnu_written = set()
background_tokens = {name: 'AEGIS_BG_'+secrets.token_hex(12) for name in names}
backgrounds = {}
users = {}
events = []
master = None
client = None
pending = bytearray()
shell_active = False
held_login = None
package_prompt = False
ce_holders = {}
mq_prefix = 'aegis_' + secrets.token_hex(8)
mq_created = set()
GNU_PROMPT = "__AEGIS_GNU_"+secrets.token_hex(12)+"> "

def clean(data):
    for value in credentials.values():
        if value in data:
            raise RuntimeError('Credential echo detected; raw output is not recorded')
    return data.decode('utf-8', errors='replace').replace('\r', '')

def record(action, text):
    entry = {'action': action, 'utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
             'output': clean(text) if isinstance(text, (bytes, bytearray)) else text}
    events.append(entry)
    (OUT/'events.json').write_text(json.dumps(events, indent=2, ensure_ascii=False)+'\n')
    print(json.dumps(entry, ensure_ascii=False), flush=True)

def until(marker, timeout=180):
    token = marker.encode()
    deadline = time.monotonic() + timeout
    while token not in pending:
        if time.monotonic() >= deadline:
            raise TimeoutError('CLI prompt not observed: '+marker)
        ready, _, _ = select.select([master], [], [], min(1, max(0, deadline-time.monotonic())))
        if ready:
            chunk = os.read(master, 65536)
            if not chunk: raise RuntimeError('CLI terminal closed')
            pending.extend(chunk)
        if len(pending) > 1024*1024: raise RuntimeError('Unbounded terminal output')
    end = pending.index(token)+len(token)
    result = pending[:end]
    del pending[:end]
    clean(result)
    return result

def close():
    global client, master, shell_active, package_prompt
    if client and client.poll() is None:
        # A failed assertion may leave the remote terminal at a password
        # prompt. Never send a shell command as an unintended credential.
        # Explicit graceful exit has its separate exit-receipt control.
        client.terminate()
        try: client.wait(timeout=5)
        except subprocess.TimeoutExpired:
            client.kill(); client.wait(timeout=5)
    if master is not None: os.close(master)
    client = master = None
    pending.clear()
    shell_active=False
    package_prompt=False

def open_client():
    global master, client
    close()
    digest = checked_output(ADB+['shell', 'getprop', 'ro.boot.vbmeta.digest'], timeout=15).decode().strip()
    receipt = json.loads((PREPARED/'avb-checked.json').read_text())
    assert receipt['builder_commit'] == COMMIT
    assert digest == receipt['vbmeta_digest'], 'Refuse a different guest image'
    profile=json.loads((PROFILE/'profile.json').read_text())
    expected_profile=(OUT/'expected-profile-id.txt').read_text().strip()
    assert profile['profile_id']==expected_profile
    assert checked_output(ADB+['shell','getprop','ro.aegis.runtime.mode'],timeout=15).strip()==b'managed-v1'
    assert checked_output(ADB+['shell','getprop','init.svc.aegis-runtime-broker'],timeout=15).strip()==b'running'
    assert checked_output(ADB+['shell', 'getenforce'], timeout=15).strip() == b'Enforcing'
    boot = checked_output(ADB+['shell', 'getprop', 'sys.boot_completed'], timeout=15).strip()
    if boot != b'1': raise RuntimeError('Guest Android boot is not complete')
    run_control(ADB+['shell','input','keyevent','KEYCODE_WAKEUP'],check=True,timeout=15)
    master, slave = pty.openpty()
    client = subprocess.Popen(ADB+['shell', '-tt', 'su', '0', '/system_ext/bin/aegis'],
                              stdin=slave, stdout=slave, stderr=slave, close_fds=True)
    os.close(slave)
    fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack('HHHH', 28, 92, 0, 0))
    record('open', until('aegis> '))

def auth_state(key):
    uid,serial=users[key]
    def read(command):
        return checked_output(ADB+['shell','su 0 sh -c '+shlex.quote(command)],text=True,timeout=20).strip()
    ce=read('dumpsys mount | grep "^CE unlocked users: "')
    context=read('if test -d /sys/fs/cgroup/aegis-runtime/contexts/u'+str(uid)+'-s'+str(serial)+'; then echo present; else echo absent; fi')
    return {'target':list(users[key]),'ce':ce,'target_unlocked':uid in [int(x) for x in re.findall(r'\d+',ce)],
            'context':context,'foreground':read('am get-current-user')}

def check_prepared(key,before):
    after=auth_state(key)
    assert after['foreground']==str(users[key][0]), 'Prepared login target is not foreground'
    # Completing a previously pending eviction may remove the old cached CE
    # state. Preparation must never grant new CE access without a password.
    assert not after['target_unlocked'] or before['target_unlocked'], 'Login preparation unlocked target CE before password'
    assert after['context']==before['context'], 'Login preparation changed GNU context before password'
    record('prepared-before-password-'+key,{'before':before,'after':after,
        'scope':'No password sent yet. Foreground target is selected; no new CE access and GNU context unchanged. Pending eviction may finish locking CE. Completed switch is enforced by service, not inferred from foreground alone.'})


def action(label, command, prompts=()):
    if not client or client.poll() is not None: raise RuntimeError('Open the CLI first')
    auth_key = ('alpha' if label in ('login-a','wrong-a') else 'beta') if prompts and label in ('login-a','wrong-a','switch-b','wrong-b','old-b','login-b-new') else None
    auth_before = auth_state(auth_key) if auth_key is not None else None
    os.write(master, command.encode()+b'\n')
    response = bytearray()
    for prompt, key in prompts:
        before = auth_before
        response.extend(until(prompt))
        if before is not None: check_prepared(auth_key,before)
        # The Java Console owns the guest no-echo terminal; no shell password argument.
        os.write(master, credentials[key]+b'\n')
    response.extend(until('aegis> '))
    record(label, response)
    for line in clean(response).splitlines():
        match = re.match(r'user=(\d+) serial=(\d+) name=(.*?) admin=', line)
        if not match: continue
        for key, display in names.items():
            if match[3] == display:
                identity = (int(match[1]), int(match[2]))
                if key in users and users[key] != identity:
                    raise RuntimeError('Test identity number or serial was replaced')
                if not 10 <= identity[0] <= 21472: raise RuntimeError('Unexpected personal ID')
                users[key] = identity

def probe_file(operation, key):
    # A developer-root CE readback test, not a runtime/ordinary-user isolation proof.
    uid, serial = users[key]
    folder = f'/data/misc_ce/{uid}/aegis-cli-smoke'
    path = folder+'/probe.bin'
    payload = b''
    if operation == 'write':
        command = f'umask 077; test ! -e {folder} && mkdir {folder} && cat > {path}'
        payload = probes[key]
    else: command = 'cat '+path
    result = run_control(ADB+['shell', '-T', 'su', '0', 'sh', '-c', shlex.quote(command)],
                            input=payload, capture_output=True, timeout=30)
    if operation == 'locked':
        assert result.returncode != 0 and result.stdout != probes[key], 'CE remained readable'
        assert not result.stdout, 'Locked CE read returned bytes'
        assert (b'Required key not available' in result.stderr or b'No such file or directory' in result.stderr), 'Unexpected CE read failure'
        mount_state=checked_output(ADB+['shell','dumpsys','mount'],text=True,timeout=20)
        unlocked=[line for line in mount_state.splitlines() if line.startswith('CE unlocked users: ')]
        assert len(unlocked)==1
        assert uid not in [int(number) for number in re.findall(r'\d+',unlocked[0])], 'AOSP still reports CE unlocked'
        record('locked-read-evidence-'+key, {'read_exit':result.returncode,'stdout_bytes':len(result.stdout),
               'stderr':result.stderr.decode(errors='replace'),'aosp_ce_state':unlocked[0],
               'scope':'Requires later identical-byte read after reauthentication; not alone a runtime isolation proof'})
    else:
        assert result.returncode == 0, 'CE operation failed'
        if operation == 'read': assert result.stdout == probes[key], 'CE bytes changed'
    record(operation+'-'+key, {'user':uid, 'serial':serial, 'bytes':len(probes[key]),
                              'sha256':hashlib.sha256(probes[key]).hexdigest(),
                              'scope':'Developer-root CE test; no Linux runtime isolation claim'})


def start_shell():
    global shell_active
    assert not shell_active
    os.write(master, b'linux shell\n')
    # Read the actual initial Bash prompt; returning to aegis> is a failure.
    deadline = time.monotonic()+40
    while not re.search(rb'(?:\$ |# |aegis> )$', pending):
        assert time.monotonic()<deadline, 'No shell prompt observed'
        ready, _, _ = select.select([master], [], [], 1)
        if ready:
            chunk=os.read(master,65536)
            assert chunk, 'Channel closed'
            pending.extend(chunk)
        assert len(pending)<1024*1024
    data=bytes(pending);pending.clear();record('shell-entry',data)
    assert not data.endswith(b'aegis> '), 'Runtime did not enter Bash'
    shell_active=True
    # The complete expected marker does not occur in echoed input.
    half=len(GNU_PROMPT)//2
    line="bind 'set enable-bracketed-paste off'; PS1="+shlex.quote(GNU_PROMPT[:half])+shlex.quote(GNU_PROMPT[half:])
    os.write(master,line.encode()+b'\n')
    record('gnu-prompt',until(GNU_PROMPT,20))

def gnu(label, command):
    assert shell_active
    os.write(master,command.encode()+b'\n')
    data=until(GNU_PROMPT,30)
    record(label,data)
    return clean(data)


def checked_gnu(label, command):
    # The nonce appears only in split form in echoed input; accept one actual
    # newline-delimited exit record from the guest, not an echoed command.
    marker='__AEGIS_GNU_STATUS_'+secrets.token_hex(12)
    half=len(marker)//2
    line='(set -eu; '+command+'); _aegis_test_rc=$?; '
    line+="printf '\\n%s%s:%d\\n' "+shlex.quote(marker[:half])+' '+shlex.quote(marker[half:])+' "$_aegis_test_rc"'
    assert shell_active
    os.write(master,line.encode()+b'\n')
    data=until(marker+':',30)+until('\n',5)+until(GNU_PROMPT,5)
    record(label,data)
    # Readline emits these mode toggles next to the first output byte. Keep
    # the original terminal transcript in record(), but normalize only the
    # protocol toggles for line-oriented checks such as a GNU checksum.
    output=clean(data).replace('\x1b[?2004h','').replace('\x1b[?2004l','')
    matches=re.findall(r'(?m)^'+re.escape(marker)+r':([0-9]+)$',output)
    assert matches==['0'], 'GNU command failed or completion record missing'
    return output


def basic_runtime():
    # Every command executes in the authenticated personal GNU shell. The
    # nonce/exit-record verifier must confirm execution rather than input echo.
    commands = [
        ('gnu-identity', 'test "$(id -u)" = 1000; test "$(id -g)" = 1000; test "$HOME" = /home/user; test "$(pwd)" = /home/user; id'),
        ('gnu-tools', 'command -v bash; command -v apt; command -v dpkg; command -v ls; getconf GNU_LIBC_VERSION; cat /etc/debian_version; uname -a'),
        ('gnu-domain', 'test "$(cat /proc/self/attr/current)" = u:r:aegis_runtime_program:s0; cat /proc/self/attr/current'),
        ('gnu-readonly-base', 'if touch /usr/aegis-write-must-fail 2>/tmp/aegis-base-error; then exit 91; fi; cat /tmp/aegis-base-error; rm /tmp/aegis-base-error'),
        ('gnu-namespaces', 'for n in user mnt pid ipc uts net; do readlink /proc/self/ns/$n; done; cat /proc/self/uid_map; cat /proc/self/gid_map'),
        ('gnu-ephemeral', 'umask 077; mkdir /tmp/aegis-context-probe; printf temporary > /tmp/aegis-context-probe/value; test "$(cat /tmp/aegis-context-probe/value)" = temporary; test -d /run/user/1000; stat -c %u:%g:%a /home/user /run/user/1000'),
    ]
    for label, command in commands: checked_gnu(label, command)
    layout=checked_gnu('gnu-mount-layout', 'test ! -e /proc/sys; test ! -e /sys/fs/cgroup; test -z "$(ls -A /sys)"; cat /proc/self/mountinfo')
    mounts={}
    for line in layout.splitlines():
        if not re.match(r'^\d+ \d+ \d+:\d+ ',line): continue
        left,right=line.split(' - ',1);fields=left.split();filesystem=right.split()[0]
        assert fields[4] not in mounts, 'Duplicate guest mount'
        assert not any(x.startswith(('shared:','master:')) for x in fields[6:])
        mounts[fields[4]]={'filesystem':filesystem,'flags':set(fields[5].split(','))}
    expected={'/':('ext4',{'ro','nosuid','nodev'},{'noexec'}),
        '/home/user':('f2fs',{'rw','nosuid','nodev'},{'noexec'}),
        '/dev':('tmpfs',{'ro','nosuid','noexec'},{'nodev'}),
        '/proc':('proc',{'rw','nosuid','nodev','noexec'},set()),
        '/dev/pts':('devpts',{'rw','nosuid','noexec'},{'nodev'}),
        '/dev/mqueue':('mqueue',{'rw','nosuid','nodev','noexec'},set()),
        '/dev/shm':('tmpfs',{'rw','nosuid','nodev','noexec'},set()),
        '/tmp':('tmpfs',{'rw','nosuid','nodev'},{'noexec'}),
        '/run':('tmpfs',{'rw','nosuid','nodev','noexec'},set())}
    assert set(mounts)==set(expected), 'Unexpected inherited or missing guest mount'
    for path,(filesystem,required,forbidden) in expected.items():
        actual=mounts[path]
        assert actual['filesystem']==filesystem and required<=actual['flags'] and not forbidden&actual['flags'],path
    status=checked_gnu('gnu-process-confinement', 'cat /proc/self/status')
    for field,value in {'CapInh':'0000000000000000','CapPrm':'0000000000000000',
        'CapEff':'0000000000000000','CapBnd':'0000000000000000','CapAmb':'0000000000000000',
        'NoNewPrivs':'1','Seccomp':'2'}.items():
        assert re.findall(r'(?m)^'+field+r':\s*(\S+)$',status)==[value],field
    record('basic-runtime', {'status':'GNU_COMMANDS_EXECUTED_IN_PERSONAL_CONTEXT',
           'limits':['Single-user checks only; no two-user isolation, package management or reboot proof']})



def gnu_file(operation, key):
    assert shell_active
    other = 'beta' if key == 'alpha' else 'alpha'
    marker = '$HOME/.aegis-proof/' + gnu_files[key]
    peer_marker = '$HOME/.aegis-proof/' + gnu_files[other]
    value = gnu_probes[key]
    if operation == 'write':
        command = 'umask 077; test ! -e "' + marker + '"; mkdir -p "$HOME/.aegis-proof"; '
        command += "printf '%s' " + shlex.quote(value) + ' > "' + marker + '"; '
    else:
        command = ''
    command += 'test "$(cat "' + marker + '")" = ' + shlex.quote(value) + '; '
    command += 'test ! -e "' + peer_marker + '"; '
    command += 'test "$(stat -c %u:%g:%a "' + marker + '")" = 1000:1000:600; '
    command += 'sha256sum "' + marker + '"'
    output = checked_gnu('gnu-'+operation+'-'+key, command)
    expected = hashlib.sha256(value.encode()).hexdigest()
    assert re.search(r'(?m)^'+expected+r'  /home/user/\.aegis-proof/'+gnu_files[key]+r'$', output)
    if operation == 'write': gnu_written.add(key)
    record('gnu-file-proof-'+key, {'operation':operation, 'identity':users[key],
        'bytes':len(value), 'sha256':expected, 'path':'/home/user/.aegis-proof/'+gnu_files[key],
        'peer_marker_absent':gnu_files[other], 'scope':'Actual authenticated GNU process, not developer-root file I/O'})


def locked_gnu_file(key):
    # Test the very file previously written by GNU, not a path that might never
    # have existed. Later gnu-read must prove identical bytes after login.
    assert key in gnu_written
    uid,serial=users[key]
    path=f'/data/misc_ce/{uid}/aegis/home/.aegis-proof/{gnu_files[key]}'
    result=run_control(ADB+['shell','-T','su 0 sh -c '+shlex.quote('cat '+shlex.quote(path))],
                       capture_output=True,timeout=20)
    assert result.returncode!=0 and result.stdout==b''
    assert b'Required key not available' in result.stderr or b'No such file or directory' in result.stderr
    mount=checked_output(ADB+['shell','dumpsys','mount'],text=True,timeout=20)
    rows=[line for line in mount.splitlines() if line.startswith('CE unlocked users: ')]
    assert len(rows)==1 and uid not in [int(x) for x in re.findall(r'\d+',rows[0])]
    record('locked-gnu-file-'+key, {'user':uid,'serial':serial,'exit_code':result.returncode,
           'stdout_bytes':0,'stderr':result.stderr.decode(errors='replace'),
           'aosp_ce_state':rows[0],'expected_sha256':hashlib.sha256(gnu_probes[key].encode()).hexdigest(),
           'scope':'Previously GNU-written file is unreadable; identical-byte GNU read after reauthentication remains required'})


def held_ce_file(operation, key):
    """Controlled developer-root FD fault; never evidence of unprivileged isolation."""
    assert key in gnu_written and not shell_active
    if operation == 'start':
        assert key not in ce_holders
        uid, serial = users[key]
        path = f'/data/misc_ce/{uid}/aegis/home/.aegis-proof/{gnu_files[key]}'
        gate = '/data/local/aegis-debug/hold-' + secrets.token_hex(16)
        # Bound the fixture itself to three minutes even if the driver disappears.
        command = ('umask 077; exec 9<' + shlex.quote(path) + ' || exit 1; '
                   'touch ' + shlex.quote(gate) + '; echo AEGIS_FD_HELD; '
                   'n=0; while test -e ' + shlex.quote(gate) + ' && test "$n" -lt 1800; '
                   'do sleep 0.1; n=$((n+1)); done; exec 9<&-; '
                   'rm -f ' + shlex.quote(gate) + '; echo AEGIS_FD_RELEASED')
        process = subprocess.Popen(ADB + ['shell', '-T', 'su 0 sh -c ' + shlex.quote(command)],
                stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        ce_holders[key] = {'process': process, 'gate': gate,
            'system_server': checked_output(ADB+['shell','pidof','system_server'], timeout=20).strip()}
        ready, _, _ = select.select([process.stdout], [], [], 20)
        assert ready and process.stdout.readline().strip() == b'AEGIS_FD_HELD'
        record('held-ce-start-'+key, {'user':uid,'serial':serial,
            'scope':'Developer-root holds an existing GNU-written CE file open for fault injection only'})
    else:
        holder = ce_holders[key]
        run_control(ADB+['shell','su','0','rm','-f',holder['gate']],check=True,timeout=20)
        output, error = holder['process'].communicate(timeout=20)
        assert holder['process'].returncode == 0 and output.strip() == b'AEGIS_FD_RELEASED'
        assert checked_output(ADB+['shell','pidof','system_server'],timeout=20).strip() == holder['system_server']
        del ce_holders[key]
        record('held-ce-release-'+key, {'original_system_server_survived':True,
            'scope':'The original fixture closed its FD; CE completion and fresh authentication still required'})


def mqueue_probe(operation, key):
    # ARM64 kernel syscall numbers, shared by the pinned Android and GNU ABI.
    # Linux mq_open's raw syscall takes the name WITHOUT glibc's leading '/'.
    assert shell_active
    peer = 'beta' if key == 'alpha' else 'alpha'
    common, own, foreign = mq_prefix+'_common', mq_prefix+'_'+key, mq_prefix+'_'+peer
    value = 'MESSAGE_'+key
    code = 'use strict; use warnings; '
    if operation == 'create':
        code += (f'for my $entry ("{common}","{own}") {{ my $name="$entry"; '
                 'my $fd=syscall(180,$name,194,0600,0); die "mq_open:$!" if $fd<0; '
                 f'my $data="{value}"; '
                 'die "mq_send:$!" if syscall(182,$fd,$data,length($data),0,0)!=0; '
                 'die "close:$!" if syscall(57,$fd)!=0; } print "MQUEUE_CREATED\\n";')
    elif operation == 'isolation':
        assert key in mq_created and peer in mq_created
        code += (f'my $foreign="{foreign}"; '
                 'die "foreign queue accessible" if syscall(180,$foreign,2048,0,0)>=0; '
                 'die "wrong denial:$!" if 0+$! != 2; '
                 f'my $name="{common}"; my $fd=syscall(180,$name,2048,0,0); '
                 'die "own open:$!" if $fd<0; my $data="\\0"x8192; my $prio="\\0"x4; '
                 'my $n=syscall(183,$fd,$data,8192,$prio,0); die "receive:$!" if $n<0; '
                 f'die "wrong user message" if substr($data,0,$n) ne "{value}"; '
                 'die "close:$!" if syscall(57,$fd)!=0; print "MQUEUE_PRIVATE_MESSAGE_AND_PEER_DENIAL\\n";')
    elif operation == 'empty':
        assert key in mq_created
        code += (f'for my $entry ("{common}","{own}") {{ my $name="$entry"; '
                 'die "old queue survived" if syscall(180,$name,2048,0,0)>=0; '
                 'die "wrong denial:$!" if 0+$! != 2; } print "MQUEUE_OLD_NAMESPACE_GONE\\n";')
    else:
        raise ValueError('Unknown mqueue probe')
    checked_gnu('mqueue-'+operation+'-'+key,
                'test "$(uname -m)" = aarch64; perl -e '+shlex.quote(code))
    if operation == 'create': mq_created.add(key)
    record('mqueue-'+operation+'-proof-'+key, {'user':users[key],
           'scope':'Actual unprivileged GNU Perl in the personal IPC namespace; raw ARM64 POSIX-mqueue syscalls'})


def until_any(markers, timeout=600):
    deadline=time.monotonic()+timeout
    encoded=[x.encode() for x in markers]
    while True:
        hits=[(pending.index(token),i,token) for i,token in enumerate(encoded) if token in pending]
        if hits:
            pos,i,token=min(hits);end=pos+len(token);result=pending[:end];del pending[:end]
            clean(result);return i,result
        if time.monotonic()>=deadline:raise TimeoutError('Package state not observed')
        ready,_,_=select.select([master],[],[],1)
        if ready:
            chunk=os.read(master,65536)
            if not chunk:raise RuntimeError('CLI closed during package work')
            pending.extend(chunk)
        assert len(pending)<1024*1024

def package_begin(action_name,scope,package="hello"):
    global package_prompt
    assert not shell_active and not package_prompt and held_login is None
    assert client and client.poll() is None
    assert action_name in ('install','remove','update') and scope in ('user','all')
    assert re.fullmatch(r'[a-z0-9][a-z0-9+.-]{0,62}(=(?:[0-9]+:)?[0-9][a-zA-Z0-9.+~-]{0,100})?',package)
    assert action_name=='install' or '=' not in package
    command='linux package '+action_name+' --scope '+scope+(' '+package if action_name!='update' else '')
    os.write(master,command.encode()+b'\n')
    which,data=until_any(['Admin-Benutzer für diesen Plan (leer bricht ab): ','aegis> '])
    record('package-'+action_name+'-'+scope,data)
    package_prompt=which==0
    if not package_prompt:raise RuntimeError('Package never reached approval; inspect native diagnostics')
    package_name=package.split('=')[0]
    assert (re.search(re.escape(package_name)+r' \((?:arm64|all)\): ',clean(data))
            or action_name=='update'
            or package_name+': privat festhalten, Version ' in clean(data))

def package_approve(key, administrator="alpha"):
    global package_prompt
    assert package_prompt and client and client.poll() is None
    os.write(master,names[administrator].encode()+b'\n')
    data=until('Admin-Passwort für diesen Plan: ',30)
    os.write(master,credentials[key]+b'\n')
    data.extend(until('aegis> ',600))
    package_prompt=False
    record('package-approve-'+key,data)

def package_cancel():
    global package_prompt
    assert package_prompt and client and client.poll() is None
    os.write(master,b'\n');record('package-cancel-plan',until('aegis> ',30));package_prompt=False

def resize(rows, columns):
    assert master is not None and client and client.poll() is None
    fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack('HHHH',rows,columns,0,0))
    # openpty/Popen does not give adb a controlling PTY. Deliver its real
    # window-change signal explicitly; checked_gnu skips asynchronous redraws
    # until a unique actual completion record, then consumes the new prompt.
    os.kill(client.pid, signal.SIGWINCH)
    time.sleep(1)
    data=checked_gnu('gnu-resize', 'test "$(stty size)" = '+shlex.quote(str(rows)+' '+str(columns))+'; stty size')
    assert re.search(r'(?m)^'+str(rows)+' '+str(columns)+r'$',data)



def start_background(key):
    """Start a real GNU job, with no inherited terminal and a bounded lifetime."""
    assert shell_active and key not in backgrounds
    token = background_tokens[key]
    path = '/home/user/.aegis-proof/'+token
    # setsid may fork; the script records its actual inner PID, not shell $!.
    # Every update is an atomic rename, so readers never accept a partial row.
    script = ('set -eu; umask 077; trap "" HUP; i=0; '
              'while [ "$i" -lt 1800 ]; do i=$((i+1)); '
              'printf "%s %s %s\\n" "$0" "$BASHPID" "$i" > "$1.next"; '
              'mv "$1.next" "$1"; sleep 1; done')
    command = ('test -f "$HOME/.aegis-proof/'+gnu_files[key]+'"; '
               'test ! -e '+shlex.quote(path)+'; command -v setsid; '
               'setsid /usr/bin/bash -c '+shlex.quote(script)+' '
               +shlex.quote(token)+' '+shlex.quote(path)
               +' </dev/null >'+shlex.quote(path+'.log')+' 2>&1 &')
    # The background operator must be inside its own group; otherwise it would
    # background the verifier's whole condition chain and hide launch failures.
    command = command.replace('; setsid ', '; { setsid ')+ ' }; '
    command += ('for _aegis_wait in 1 2 3 4 5; do '
                'test -s '+shlex.quote(path)+' && break; sleep 1; done; '
                'read _aegis_token _aegis_pid _aegis_tick < '+shlex.quote(path)+'; '
                'test "$_aegis_token" = '+shlex.quote(token)+'; '
                'kill -0 "$_aegis_pid"; cat '+shlex.quote(path))
    output=checked_gnu('gnu-background-start-'+key, command)
    rows=re.findall(r'(?m)^'+re.escape(token)+r' ([0-9]+) ([0-9]+)$',output)
    assert len(rows)==1 and int(rows[0][0])>1 and int(rows[0][1])>0
    backgrounds[key]={'inner_pid':int(rows[0][0]),'tick':int(rows[0][1]),
                      'token':token,'started_monotonic':time.monotonic()}
    record('background-start-'+key, {'inner_pid':int(rows[0][0]),
           'token':token,'bounded_seconds':1800,'scope':'Real GNU process; survival checks remain pending'})



def renew_background(key):
    """Create a fresh liveness probe only after the original process is proven gone."""
    assert shell_active and key in backgrounds
    previous=backgrounds[key]
    assert {'host_pid','starttime','boot_id'} <= set(previous)
    boot=checked_output(ADB+['shell','cat','/proc/sys/kernel/random/boot_id'],text=True,timeout=20).strip()
    if boot==previous['boot_id']:
        path='/proc/'+str(previous['host_pid'])+'/stat'
        value=checked_output(ADB+['shell','su 0 sh -c '+shlex.quote(
            'if test -r '+path+'; then cat '+path+'; fi')],text=True,timeout=20).strip()
        assert not value or value.rsplit(') ',1)[1].split()[19]!=previous['starttime'], 'Original probe still exists'
    else:
        checkpoint=json.loads((OUT/'reboot-checkpoint.json').read_text())
        assert checkpoint['status']=='BOTH_GNU_CONTEXTS_REMOVED_CE_LOCKED_BEFORE_REBOOT'
        assert checkpoint['boot_id']==previous['boot_id'] and checkpoint['profile_id']==PROFILE_ID
    # Positive byte readback must precede a replacement probe; never rewrite the
    # previous marker or silently reinterpret a numeric PID from another boot.
    gnu_file('read',key)
    record('background-retired-before-renewal-'+key,dict(previous))
    del backgrounds[key]
    background_tokens[key]='AEGIS_BG_'+secrets.token_hex(12)
    start_background(key)


def observe_background(key, gone=False):
    """Developer-root oracle; never a substitute for another user's denial."""
    job=backgrounds[key]
    assert time.monotonic()-job['started_monotonic']<1500, 'Job may have expired naturally'
    uid,serial=users[key]
    token=job['token']
    boot_id=checked_output(ADB+['shell','cat','/proc/sys/kernel/random/boot_id'],text=True,timeout=20).strip()
    if 'boot_id' in job: assert job['boot_id']==boot_id, 'Process identity belongs to another guest boot'
    job['boot_id']=boot_id
    path=f'/data/misc_ce/{uid}/aegis/home/.aegis-proof/{token}'
    def root(command):
        return checked_output(ADB+['shell','su 0 sh -c '+shlex.quote(command)],
                              text=True,timeout=20).strip()
    if gone:
        assert 'host_pid' in job and 'starttime' in job
        stat=root('if test -r /proc/'+str(job['host_pid'])+'/stat; then cat /proc/'+str(job['host_pid'])+'/stat; fi')
        assert not stat or stat.rsplit(') ',1)[1].split()[19]!=job['starttime'], 'Original GNU job still exists'
        assert root('test ! -d /sys/fs/cgroup/aegis-runtime/contexts/u'+str(uid)+'-s'+str(serial)+' && echo removed')=='removed'
        unlocked=root('dumpsys mount | grep "^CE unlocked users: "')
        assert uid not in [int(x) for x in re.findall(r'\d+',unlocked)]
        record('background-logout-'+key, {'original_host_pid':job['host_pid'],
               'starttime':job['starttime'],'aosp_ce_state':unlocked,
               'scope':'Original process gone, personal context removed, AOSP reports CE locked; later byte readback still required'})
        return
    first=root('cat '+shlex.quote(path)).split()
    assert len(first)==3 and first[0]==token and int(first[1])==job['inner_pid']
    time.sleep(2)
    second=root('cat '+shlex.quote(path)).split()
    assert len(second)==3 and second[:2]==first[:2] and int(second[2])>int(first[2])
    # Match an entire NUL-separated argument among actual bash processes, not
    # the diagnostic command itself or a mere process-name substring.
    candidates=root('for p in $(pidof bash); do '
                    'if tr "\\000" "\\n" < /proc/$p/cmdline | grep -Fxq '
                    +shlex.quote(token)+'; then echo "$p"; fi; done').splitlines()
    assert len(candidates)==1 and candidates[0].isdigit()
    pid=int(candidates[0]); status=root('cat /proc/'+str(pid)+'/status')
    fields=dict(line.split(':',1) for line in status.splitlines() if ':' in line)
    assert [int(x) for x in fields['Uid'].split()]==[uid*100000+7500]*4
    assert [int(x) for x in fields['NSpid'].split()]==[pid,job['inner_pid']]
    starttime=root('cat /proc/'+str(pid)+'/stat').rsplit(') ',1)[1].split()[19]
    if 'host_pid' in job: assert job['host_pid']==pid and job['starttime']==starttime
    job.update(host_pid=pid,starttime=starttime,tick=int(second[2]))
    cgroup=root('cat /proc/'+str(pid)+'/cgroup')
    assert '0::/aegis-runtime/contexts/u'+str(uid)+'-s'+str(serial) in cgroup.splitlines()
    domain=root('cat /proc/'+str(pid)+'/attr/current').rstrip('\x00')
    assert domain=='u:r:aegis_runtime_program:s0'
    namespaces=root('for n in user mnt pid ipc uts net; do printf "%s " "$n"; readlink /proc/'+str(pid)+'/ns/$n; done')
    namespace_ids=dict(line.split(' ',1) for line in namespaces.splitlines())
    assert set(namespace_ids)=={'user','mnt','pid','ipc','uts','net'}
    job['namespaces']=namespace_ids
    record('background-observed-'+key, {'user':uid,'serial':serial,'host_pid':pid,
           'inner_pid':job['inner_pid'],'host_uid':uid*100000+7500,'starttime':starttime,
           'tick_before':int(first[2]),'tick_after':int(second[2]),'cgroup':cgroup,
           'domain':domain,'namespaces':namespaces,
           'foreground_user':root('am get-current-user'),
           'power_state':root('dumpsys power | grep "mWakefulness="'),
           'scope':'Positive liveness/origin oracle; cross-user file/process denial must be tested from GNU separately'})


def check_peer_isolation(key):
    """Use two live GNU jobs as the positive controls for actual denied access."""
    assert shell_active
    peer='beta' if key=='alpha' else 'alpha'
    assert key in gnu_written and peer in gnu_written
    gnu_file('read',key)
    observe_background(key)
    observe_background(peer)
    own,other=backgrounds[key],backgrounds[peer]
    assert own['host_pid']!=other['host_pid'] and users[key][0]!=users[peer][0]
    assert all(own['namespaces'][n]!=other['namespaces'][n] for n in own['namespaces'])
    peer_id=users[peer][0]
    paths=[f'/data/misc_ce/{peer_id}/aegis/home/.aegis-proof/{gnu_files[peer]}',
           '/home/user/.aegis-proof/'+gnu_files[peer],
           f'/proc/{other["host_pid"]}/root/home/user/.aegis-proof/{gnu_files[peer]}']
    command='umask 077; '
    for path in paths:
        command+=('if cat '+shlex.quote(path)+' > /tmp/aegis-peer-denied 2>/tmp/aegis-peer-error; '
                  'then exit 91; fi; test ! -s /tmp/aegis-peer-denied; cat /tmp/aegis-peer-error; ')
    command+=('rm /tmp/aegis-peer-denied /tmp/aegis-peer-error; '
              'test ! -e /proc/'+str(other['host_pid'])+'/stat; '
              'if kill -STOP '+str(other['host_pid'])+' 2>/tmp/aegis-peer-signal; '
              'then exit 92; fi; cat /tmp/aegis-peer-signal; rm /tmp/aegis-peer-signal; '
              'for _p in /proc/[0-9]*; do '
              'if read _comm < "$_p/comm" 2>/dev/null && test "$_comm" = bash; then '
              'if tr "\\000" "\\n" < "$_p/cmdline" 2>/dev/null | grep -Fxq '
              +shlex.quote(other['token'])+'; then exit 93; fi; fi; done')
    checked_gnu('gnu-peer-denials-'+key,command)
    # A failed signal alone could mean the target never existed. Confirm the
    # original peer process is still alive and making progress after the probe.
    observe_background(peer)
    record('two-live-context-isolation-'+key, {'requester':users[key],
           'peer':users[peer],'separate_namespaces':sorted(own['namespaces']),
           'peer_host_pid':other['host_pid'],
           'scope':'Actual GNU denied reads and SIGSTOP, with live peer before/after; not package or exhaustive syscall coverage'})

def switch_from_second_console(key):
    """Authenticate through another real CLI while the first GNU PTY is active."""
    global shell_active
    assert shell_active
    previous='beta' if key=='alpha' else 'alpha'
    assert key in users and previous in backgrounds
    current=checked_output(ADB+['shell','am','get-current-user'],text=True,timeout=20).strip()
    assert current==str(users[previous][0]), 'Expected the first GNU user in foreground'
    observe_background(previous)
    second_master,second_slave=pty.openpty()
    second=None
    buffer=bytearray()
    def read_prompt(marker,timeout=120):
        token=marker.encode();end=time.monotonic()+timeout
        while token not in buffer:
            assert time.monotonic()<end, 'Second console prompt timed out'
            if select.select([second_master],[],[],1)[0]:
                chunk=os.read(second_master,65536)
                assert chunk, 'Second console closed'
                buffer.extend(chunk)
                assert len(buffer)<1024*1024
        count=buffer.index(token)+len(token)
        output=bytes(buffer[:count]);del buffer[:count]
        clean(output)
        return output
    try:
        second=subprocess.Popen(ADB+['shell','-tt','su','0','/system_ext/bin/aegis'],
            stdin=second_slave,stdout=second_slave,stderr=second_slave,close_fds=True)
        os.close(second_slave);second_slave=None
        record('second-console-open',read_prompt('aegis> '))
        os.write(second_master,('login '+shlex.quote(names[key])+'\n').encode())
        record('second-console-auth-prompt',read_prompt('Passwort: '))
        os.write(second_master,credentials[key]+b'\n')
        output=read_prompt('aegis> ')
        record('second-console-login-'+key,output)
        assert re.search(r'(?m)^user='+str(users[key][0])+r' serial='+str(users[key][1])+r' ',clean(output))
        assert checked_output(ADB+['shell','am','get-current-user'],text=True,timeout=20).strip()==str(users[key][0])
        record('switch-revoked-first-gnu-channel',until('aegis> ',30))
        shell_active=False
        action('first-console-after-external-switch','status')
        assert 'terminal=unauthenticated' in events[-1]['output']
        observe_background(previous)
        record('active-user-switch-'+key,{'from':users[previous],'to':users[key],
            'scope':'Fresh AOSP login on a second CLI changed foreground; first active GNU PTY revoked and original background job progressed'})
    finally:
        if second_slave is not None: os.close(second_slave)
        if second is not None and second.poll() is None:
            os.write(second_master,b'exit\n')
            try: second.wait(timeout=10)
            except subprocess.TimeoutExpired:
                second.terminate();second.wait(timeout=5)
        os.close(second_master)


def expect_ce(which):
    expected={0}
    choices={'base':(), 'a':('alpha',), 'b':('beta',), 'both':('alpha','beta')}
    assert which in choices
    for key in choices[which]: expected.add(users[key][0])
    mount=checked_output(ADB+['shell','dumpsys','mount'],text=True,timeout=20)
    rows=[line for line in mount.splitlines() if line.startswith('CE unlocked users: ')]
    assert len(rows)==1
    actual={int(x) for x in re.findall(r'\d+',rows[0])}
    record('ce-state-'+which,{'expected':sorted(expected),'actual':sorted(actual)})
    assert actual==expected, 'AOSP CE state differs from the requested assertion'


def reboot_checkpoint():
    # Preserve only public test identities and file hashes, never passwords.
    assert client is None and not shell_active
    assert gnu_written=={'alpha','beta'}
    def root(command):
        return checked_output(ADB+['shell','su 0 sh -c '+shlex.quote(command)],text=True,timeout=20).strip()
    assert root('dumpsys mount | grep "^CE unlocked users: "')=='CE unlocked users: [0]'
    assert root('dumpsys user | grep "Started users state:"')=='Started users state: [0=RUNNING_UNLOCKED]'
    assert 'populated 0' in root('cat /sys/fs/cgroup/aegis-runtime/contexts/cgroup.events').splitlines()
    for key in ('alpha','beta'):
        assert key in backgrounds
        observe_background(key,gone=True)
        locked_gnu_file(key)
    profile_path=PROFILE/'profile.json'
    profile=json.loads(profile_path.read_text())
    assert profile['profile_id']==(OUT/'expected-profile-id.txt').read_text().strip()
    result={'status':'BOTH_GNU_CONTEXTS_REMOVED_CE_LOCKED_BEFORE_REBOOT',
            'profile_id':profile['profile_id'],
            'image_commit':COMMIT,
            'boot_id':root('cat /proc/sys/kernel/random/boot_id'),
            'vbmeta_digest':root('getprop ro.boot.vbmeta.digest'),
            'identities':users,
            'files':{k:{'name':gnu_files[k],'sha256':hashlib.sha256(gnu_probes[k].encode()).hexdigest()} for k in gnu_written}}
    with (OUT/'reboot-checkpoint.json').open('x') as stream:
        json.dump(result,stream,indent=2);stream.write('\n')
    record('reboot-checkpoint',result)


def removal_state():
    """Read public identity metadata and key-directory presence, never key contents."""
    def root(command):
        return checked_output(ADB+['shell','su 0 sh -c '+shlex.quote(command)],text=True,timeout=30).strip()
    dump=root('dumpsys user')
    return {'identities':re.findall(r'UserInfo\{[^\n]+ serialNo=[^\n]+',dump),
            'started':next(x.strip() for x in dump.splitlines() if 'Started users state:' in x),
            'ce':root('dumpsys mount | grep "^CE unlocked users: "'),
            'ce_key_ids':root('ls -1 /data/misc/vold/user_keys/ce'),
            'de_key_ids':root('ls -1 /data/misc/vold/user_keys/de'),
            'contexts':root('find /sys/fs/cgroup/aegis-runtime/contexts -mindepth 1 -maxdepth 1 -type d')}


def removal_denied(kind):
    assert not shell_active and held_login is None and set(users)=={'alpha','beta'}
    action('removal-authority-before-'+kind,'status')
    status=events[-1]['output']
    actor='beta' if kind=='nonadmin' else 'alpha'
    if kind=='unauthenticated':
        assert 'terminal=unauthenticated' in status
    else:
        assert re.search(r'(?m)^user='+str(users[actor][0])+r' serial='+str(users[actor][1])+r' ',status)
        assert 'foreground=true running=true ce=unlocked' in status
        assert ('admin=false' if kind=='nonadmin' else 'admin=true') in status
    target='alpha' if kind in ('self','nonadmin') else 'beta'
    before=removal_state()
    action('remove-denied-'+kind,'user remove '+shlex.quote(names[target]),
           [('Adminpasswort für die Löschung dieses Benutzers und seiner Daten: ',
             'wrong' if kind=='wrong-password' else current_credentials[actor])])
    response=events[-1]['output']
    expected='AOSP hat das Passwort abgewiesen.' if kind=='wrong-password' else 'Aktion abgelehnt.'
    assert expected in response and 'AOSP user absent' not in response
    after=removal_state()
    assert before==after,'Denied removal changed user, key-directory or runtime state'
    record('remove-denial-state-unchanged-'+kind,after)


def remove_beta():
    assert not shell_active and held_login is None and set(users)=={'alpha','beta'}
    required={'remove-denial-state-unchanged-'+x for x in ('unauthenticated','nonadmin','self','wrong-password')}
    assert required <= {e['action'] for e in events},'Run the four real permission checks first'
    action('removal-authority-before-success','status')
    assert re.search(r'(?m)^user='+str(users['alpha'][0])+r' serial='+str(users['alpha'][1])+r' ',events[-1]['output'])
    assert 'admin=true' in events[-1]['output'] and 'foreground=true running=true ce=unlocked' in events[-1]['output']
    observe_background('alpha');observe_background('beta')
    before=removal_state()
    action('remove-beta-with-fresh-admin-password','user remove '+shlex.quote(names['beta']),
           [('Adminpasswort für die Löschung dieses Benutzers und seiner Daten: ','alpha')])
    assert 'AOSP user absent; user stopped and CE storage locked' in events[-1]['output']
    assert 'runtime=removed' in events[-1]['output']
    uid,serial=users['beta']
    assert 'user='+str(uid)+' serial='+str(serial) in events[-1]['output']
    after=removal_state()
    assert not any('UserInfo{'+str(uid)+':' in row for row in after['identities'])
    assert uid not in [int(x) for x in re.findall(r'\d+',after['ce'])]
    assert str(uid) not in after['ce_key_ids'].splitlines() and str(uid) not in after['de_key_ids'].splitlines()
    assert '/u'+str(uid)+'-s'+str(serial) not in after['contexts']
    paths=[f'/data/{part}/{uid}' for part in ('misc_ce','misc_de','user','user_de','system_ce','system_de','system/users')]
    paths += [f'/data/system/users/{uid}.xml'+suffix for suffix in ('','.backup','.reservecopy')]
    for path in paths:
        result=run_control(ADB+['shell','su 0 stat '+shlex.quote(path)],text=True,capture_output=True,timeout=20)
        assert result.returncode!=0 and 'No such file or directory' in result.stderr and 'Permission denied' not in result.stderr
    lists={}
    for suffix in ('','.backup','.reservecopy'):
        path='/data/system/users/userlist.xml'+suffix
        result=run_control(ADB+['shell','su 0 stat '+shlex.quote(path)],text=True,capture_output=True,timeout=20)
        if result.returncode:
            assert 'No such file or directory' in result.stderr and 'Permission denied' not in result.stderr
            lists[path]=None
        else:
            xml=checked_output(ADB+['shell','su','0','/system/bin/abx2xml',path,'-'],text=True,timeout=20)
            lists[path]=[entry.attrib['id'] for entry in ET.fromstring(xml).findall('user')]
            assert str(uid) not in lists[path]
    assert lists['/data/system/users/userlist.xml'] is not None
    logs=checked_output(ADB+['shell','logcat','-d','-v','brief','UserManagerService:V','*:S'],text=True,timeout=30)
    assert f'AOSP user removal committed for {uid}/{serial}' in logs
    observe_background('beta',gone=True);observe_background('alpha')
    record('managed-cli-removal-confirmed',{'before':before,'after':after,'absent_paths':paths,'user_lists':lists,
           'scope':'Fresh AOSP admin password through actual CLI; synthetic Beta removed; Alpha original job still running. No post-reboot reuse claim.'})


print('READY: fresh-profile test controls. See docs/runtime-gnu-test-driver.md. No user has been created yet.', flush=True)
try:
    for line in sys.stdin:
        cmd = line.strip()
        try:
            if package_prompt and cmd not in ('quit','close','peek','scan','package-approve','package-wrong','package-nonadmin','package-cancel-plan'):
                raise RuntimeError('Finish or cancel the pending package review before other controls')
            if cmd == 'quit': break
            if cmd == 'open': open_client()
            elif cmd == 'close': close(); record('close', 'Client closed; no AOSP logout implied')
            elif cmd in ('setup', 'resume'):
                prompt = ('Passwort des ersten Administrators: ' if cmd == 'setup' else
                          'Passwort der begonnenen Ersteinrichtung (falls gesetzt: bisheriges Passwort): ')
                action(cmd, ('setup ' if cmd == 'setup' else 'setup --resume ')+shlex.quote(names['alpha']),
                       [(prompt, 'alpha'), ('Passwort wiederholen: ', 'alpha')])
            elif cmd in ('login-a', 'wrong-a'):
                action(cmd, 'login '+shlex.quote(names['alpha']), [('Passwort: ', 'alpha' if cmd == 'login-a' else 'wrong')])
            elif cmd in ('direct-first-a','direct-first-b'):
                key='alpha' if cmd.endswith('-a') else 'beta'
                assert not shell_active and held_login is None
                prior=auth_state(key)
                assert not prior['target_unlocked'] and prior['context']=='absent'
                earlier={'login-a','wrong-a','held-login-prompt-alpha'} if key=='alpha' else {'switch-b','wrong-b','old-b','login-b-new','held-login-prompt-beta'}
                assert not any(e['action'] in earlier for e in events), 'This would not be the first login attempt'
                label='login-a' if key=='alpha' else 'switch-b'
                action(label,('login ' if key=='alpha' else 'switch ')+shlex.quote(names[key]),[('Passwort: ',key)])
                assert 'ce=unlocked' in events[-1]['output'] and 'foreground=true' in events[-1]['output']
                time.sleep(6)
                action('stable-session-'+key,'status')
                assert 'terminal=unauthenticated' not in events[-1]['output']
                assert re.search(r'(?m)^user='+str(users[key][0])+r' serial='+str(users[key][1])+r' ',events[-1]['output'])
                assert 'foreground=true running=true ce=unlocked' in events[-1]['output']
                action('linux-start','linux start')
                assert 'runtime=ready ce=unlocked' in events[-1]['output']
                start_shell()
                time.sleep(3)
                checked_gnu('first-login-real-gnu-'+key,'test "$(id -u)" = 1000; test "$HOME" = /home/user; printf "FIRST_LOGIN_GNU_OK\n"')
                record('direct-first-login-'+key,{'status':'FIRST_ATTEMPT_STILL_AUTHENTICATED_AND_GNU_EXECUTED','user':users[key],
                       'scope':'No earlier login or wrong-password warmup in this driver; six-second post-login status and delayed actual GNU completion.'})
            elif cmd in ('stable-a','stable-b'):
                key='alpha' if cmd=='stable-a' else 'beta'
                assert not shell_active
                time.sleep(6)
                action('stable-session-'+key,'status')
                assert 'terminal=unauthenticated' not in events[-1]['output']
                assert re.search(r'(?m)^user='+str(users[key][0])+r' serial='+str(users[key][1])+r' ',events[-1]['output'])
                assert 'foreground=true running=true ce=unlocked' in events[-1]['output']
            elif cmd in ('hold-login-a','hold-login-b'):
                assert held_login is None and not shell_active
                key='alpha' if cmd.endswith('-a') else 'beta'
                before=auth_state(key)
                os.write(master,('login '+shlex.quote(names[key])+'\n').encode())
                record('held-login-prompt-'+key,until('Passwort: '))
                check_prepared(key,before)
                held_login=key
            elif cmd=='submit-held':
                assert held_login is not None
                key=held_login
                os.write(master,credentials[key]+b'\n')
                record('held-login-result-'+key,until('aegis> '))
                held_login=None
            elif cmd == 'add-b':
                action(cmd, 'user add '+shlex.quote(names['beta']),
                       [('Passwort des neuen Benutzers: ', 'beta'), ('Passwort wiederholen: ', 'beta'),
                        ('Passwort des angemeldeten Administrators für diese Anlage: ', 'alpha')])
            elif cmd in ('switch-b', 'wrong-b', 'old-b', 'login-b-new'):
                key = {'switch-b':'beta', 'wrong-b':'wrong', 'old-b':'beta', 'login-b-new':'newbeta'}[cmd]
                action(cmd, 'switch '+shlex.quote(names['beta']), [('Passwort: ', key)])
            elif cmd == 'passwd-b':
                action(cmd, 'passwd', [('Bisheriges Passwort: ', 'beta'), ('Neues Passwort: ', 'newbeta'),
                                       ('Neues Passwort wiederholen: ', 'newbeta')])
                assert 'Password changed by AOSP' in events[-1]['output']
                current_credentials['beta']='newbeta'
            elif re.fullmatch(r'package-(install|remove|update)-(user|all)( [a-z0-9][a-z0-9+.-]{0,62}(=(?:[0-9]+:)?[0-9][a-zA-Z0-9.+~-]{0,100})?)?',cmd):
                words=cmd.split();_,operation,scope=words[0].split('-')
                assert len(words)==(1 if operation=='update' else 2), 'Specify a package for install/remove; no package for update'
                package_begin(operation,scope,words[1] if len(words)==2 else 'hello')
            elif cmd in ('package-approve','package-wrong'):
                package_approve(current_credentials['alpha'] if cmd=='package-approve' else 'wrong')
            elif cmd == 'package-nonadmin': package_approve(current_credentials['beta'],'beta')
            elif cmd == 'package-cancel-plan': package_cancel()
            elif cmd == 'package-unauthenticated':
                assert not shell_active and held_login is None
                action(cmd,'linux package install hello --scope user')
                assert 'Aktion abgelehnt' in events[-1]['output']
            elif cmd in ('package-status','package-cancel'):
                assert not shell_active and held_login is None
                action(cmd,'linux '+cmd.replace('-',' '))
            elif cmd.startswith('cli '):
                assert not shell_active and held_login is None
                action('cli-check',cmd[4:])
            elif cmd == 'shell': start_shell()
            elif cmd == 'basic-runtime': basic_runtime()
            elif cmd == 'home-initial':
                dirs=['Desktop','Documents','Downloads','Pictures','Videos','Music','Books','.config','.local','.cache']
                expected='\n'.join(sorted(dirs))
                command='test "$(find "$HOME" -mindepth 1 -maxdepth 1 -printf \'%f\\n\' | LC_ALL=C sort)" = '+shlex.quote(expected)+'; '
                command+='for d in '+shlex.join(dirs)+'; do test -d "$HOME/$d"; test ! -L "$HOME/$d"; test "$(stat -c %u:%g:%a "$HOME/$d")" = 1000:1000:700; done; '
                command+='find "$HOME" -mindepth 1 -maxdepth 1 -printf \'%f %u:%g:%m\\n\' | LC_ALL=C sort'
                checked_gnu('initial-home-layout',command)
            elif cmd in ('home-change-a','home-retained-a'):
                assert 'alpha' in users
                assert checked_output(ADB+['shell','am','get-current-user'],text=True,timeout=20).strip()==str(users['alpha'][0])
                command=''
                if cmd=='home-change-a':
                    command='chmod 750 "$HOME/Books"; rmdir "$HOME/.cache"; mv "$HOME/Downloads" "$HOME/Downloads-renamed"; ln -s Downloads-renamed "$HOME/Downloads"; '
                    command+='umask 077; printf %s '+shlex.quote(gnu_probes['alpha'])+' > "$HOME/.config/aegis-layout-proof"; '
                command+='test "$(stat -c %u:%g:%a "$HOME/Books")" = 1000:1000:750; test ! -e "$HOME/.cache"; '
                command+='test -L "$HOME/Downloads"; test "$(readlink "$HOME/Downloads")" = Downloads-renamed; '
                command+='test "$(cat "$HOME/.config/aegis-layout-proof")" = '+shlex.quote(gnu_probes['alpha'])+'; sha256sum "$HOME/.config/aegis-layout-proof"'
                checked_gnu(cmd,command)

            elif re.fullmatch(r'gnu-bg-start-[ab]',cmd):
                start_background('alpha' if cmd.endswith('-a') else 'beta')
            elif re.fullmatch(r'bg-(observe|gone)-[ab]',cmd):
                observe_background('alpha' if cmd.endswith('-a') else 'beta', '-gone-' in cmd)
            elif re.fullmatch(r'isolation-from-[ab]',cmd):
                check_peer_isolation('alpha' if cmd.endswith('-a') else 'beta')
            elif re.fullmatch(r'second-switch-[ab]',cmd):
                switch_from_second_console('alpha' if cmd.endswith('-a') else 'beta')
            elif cmd == 'gnu-smoke':
                gnu(cmd, 'id; pwd; uname -s; cat /etc/debian_version; stty size; cat /proc/self/attr/current')
            elif cmd == 'resize':
                resize(36,104);resize(28,92)
            elif cmd == 'ctrl-c':
                assert shell_active
                token='__FOREGROUND_'+secrets.token_hex(12)
                half=len(token)//2
                command="printf '%s%s\\n' "+shlex.quote(token[:half])+' '+shlex.quote(token[half:])+'; sleep 60'
                os.write(master,command.encode()+b'\n')
                record('foreground-start',until(token,10))
                os.write(master,b'\x03')
                record('foreground-interrupted',until(GNU_PROMPT,10))
                gnu('shell-survived-interrupt','printf "AFTER_INTERRUPT\\n"')
            elif cmd == 'shell-exit':
                assert shell_active
                os.write(master,b'exit 7\n')
                record(cmd,until('aegis> ',20));shell_active=False
            elif cmd in ('screen-off','screen-on'):
                key='KEYCODE_SLEEP' if cmd=='screen-off' else 'KEYCODE_WAKEUP'
                run_control(ADB+['shell','input','keyevent',key],check=True,timeout=20)
                if cmd=='screen-off' and shell_active:
                    record('screen-revoked-channel',until('aegis> ',20));shell_active=False
                record(cmd,'Requested Android power input; actual session status must be checked separately')
            elif re.fullmatch(r'gnu-(write|read)-[ab]', cmd):
                _, operation, key = cmd.split('-')
                gnu_file(operation, 'alpha' if key == 'a' else 'beta')
            elif re.fullmatch(r'locked-gnu-[ab]',cmd):
                locked_gnu_file('alpha' if cmd.endswith('-a') else 'beta')
            elif re.fullmatch(r'held-ce-(start|release)-[ab]',cmd):
                held_ce_file(cmd.split('-')[2], 'alpha' if cmd.endswith('-a') else 'beta')
            elif re.fullmatch(r'mqueue-(create|isolation|empty)-[ab]',cmd):
                mqueue_probe(cmd.split('-')[1], 'alpha' if cmd.endswith('-a') else 'beta')
            elif re.fullmatch(r'pending-login-[ab]',cmd):
                key='alpha' if cmd.endswith('-a') else 'beta'
                assert key in ce_holders and not shell_active
                action(cmd, 'login '+shlex.quote(names[key]))
                assert 'Aktion nicht bestätigt' in events[-1]['output']
                assert 'Passwort: ' not in events[-1]['output']
                assert auth_state(key)['context']=='absent'
                assert checked_output(ADB+['shell','pidof','system_server'],timeout=20).strip()==ce_holders[key]['system_server']
                record(cmd+'-confirmed', 'No password sent, no runtime, original system_server survived pending eviction')
            elif cmd.startswith('gnu-checked '): checked_gnu('gnu-checked',cmd[len('gnu-checked '):])
            elif cmd.startswith('gnu '): gnu('gnu-command',cmd[4:])
            elif cmd in ('linux-start', 'linux-status', 'linux-stop'):
                action(cmd, cmd.replace('-', ' '))
            elif cmd in ('status', 'logout', 'list'):
                action(cmd, 'user list' if cmd == 'list' else cmd)
            elif re.fullmatch(r'(write|read|locked)-[ab]', cmd):
                operation, key = cmd.split('-')
                probe_file(operation, 'alpha' if key == 'a' else 'beta')
            elif cmd == 'peek':
                assert master is not None
                while select.select([master], [], [], 0)[0]:
                    chunk=os.read(master,65536)
                    if not chunk: break
                    pending.extend(chunk)
                    assert len(pending)<1024*1024
                record('pending-terminal-output',bytes(pending))
            elif cmd == 'exit-receipt':
                assert not shell_active and client and client.poll() is None
                os.write(master,b'exit\n')
                record('cli-exit',until('Terminalkanal beendet. Dies meldet den AOSP-Benutzer nicht ab.',10))
                code=client.wait(timeout=10)
                record('cli-exit-status',{'code':code,'expected_previous_shell_exit':7})
                assert code==7, 'GNU exit status did not reach the client'
            elif cmd.startswith('expect-ce '): expect_ce(cmd[len('expect-ce '):])
            elif re.fullmatch(r'gnu-bg-renew-[ab]',cmd):
                renew_background('alpha' if cmd.endswith('-a') else 'beta')
            elif cmd == 'reboot-checkpoint': reboot_checkpoint()
            elif cmd in ('remove-denied-unauthenticated','remove-denied-nonadmin','remove-denied-self','remove-denied-wrong-password'):
                removal_denied(cmd[len('remove-denied-'):])
            elif cmd == 'remove-beta': remove_beta()
            elif cmd == 'scan':
                paths = [p for d in RUN.parent.glob('boot-*')
                         for p in d.glob('*.log')]
                for p in paths: clean(p.read_bytes())
                record('credential-log-scan', f'No complete credential found in {len(paths)} run logs')
            else: print('Unknown control command', flush=True)
        except Exception as exc:
            # Avoid exception repr/output that might include credentials.
            print('FAILED '+cmd+': '+type(exc).__name__, flush=True)
finally:
    close()
    for key in list(ce_holders):
        try: held_ce_file('release', key)
        except Exception: print('FD fixture cleanup unconfirmed; bounded fixture expires independently', flush=True)
    for value in credentials.values(): value[:]=b'\0'*len(value)
    print('Credential buffers cleared', flush=True)
