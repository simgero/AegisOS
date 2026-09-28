"""Bounded development-console commands; never use this API for passwords."""
import re
import secrets
import socket
import time


def execute(path, command, timeout=15):
    marker='AEGIS_CONSOLE_'+secrets.token_hex(12)
    request='\n'+command+"; _aegis_rc=$?; printf '\\n"+marker+":%s\\n' \"$_aegis_rc\"\n"
    complete=re.compile(rb'\r?\n'+marker.encode()+rb':([0-9]+)\r?\n')
    with socket.socket(socket.AF_UNIX) as connection:
        connection.settimeout(1)
        connection.connect(str(path))
        connection.sendall(request.encode())
        output=bytearray();deadline=time.monotonic()+timeout
        while time.monotonic()<deadline and len(output)<1024**2:
            try:
                data=connection.recv(65536)
            except TimeoutError:
                continue
            if not data:break
            output.extend(data)
            result=complete.search(output)
            if result:
                if int(result[1]):raise RuntimeError('Guest command failed: '+str(int(result[1])))
                # The interactive console echoes its request. Callers should not
                # treat that echo as evidence of a command's result.
                return output[:result.start()].decode(errors='replace')
        raise TimeoutError('Guest console did not confirm command completion')
