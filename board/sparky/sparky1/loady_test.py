# loady test: sends a file with lrzsz sb over /dev/ttyUSB0 into U-Boot, then reads it back with md.l.
# Usage: uv run --no-project --with pyserial python board/sparky/sparky1/loady_test.py <file> <hex addr> <baud> [sb args]
# The destination is filled with 0xa5 first. Needs U-Boot at the prompt and nothing else holding the port.
import serial, sys, time, subprocess, re, os, fcntl
f, addr, baud = sys.argv[1], int(sys.argv[2],16), int(sys.argv[3])
data = open(f,'rb').read()
s = serial.Serial(); s.port='/dev/ttyUSB0'; s.baudrate=115200; s.timeout=0.2
s.dtr=False; s.rts=False; s.open()
def send(c, cr=True):
    for ch in c.encode()+(b'\r' if cr else b''):
        s.write(bytes([ch])); time.sleep(0.0015)
def until(pat, t):
    out=b''; end=time.time()+t
    while time.time()<end and pat not in out: out+=s.read(1)
    return out.decode(errors='replace').replace('\r','')
s.write(b'\x03'); time.sleep(0.3); s.read(4096)
send(f'mw.b {addr:#x} 0xa5 {len(data):#x}'); print(until(b'=> ',10))
if baud != 115200:
    send(f'loady {addr:#x} {baud}'); print(until(b'ENTER ...',5))
    time.sleep(0.2); s.baudrate=baud; time.sleep(0.2); s.reset_input_buffer(); s.write(b'\r')
else:
    send(f'loady {addr:#x}')
print(until(b'bps...',5))
fd=s.fileno(); fl=fcntl.fcntl(fd,fcntl.F_GETFL); fcntl.fcntl(fd,fcntl.F_SETFL,fl&~os.O_NONBLOCK)
t0=time.time()
r=subprocess.run(['sb','--ymodem','-vv']+sys.argv[4:]+[f], stdin=fd, stdout=fd, stderr=subprocess.PIPE)
dt=time.time()-t0
fcntl.fcntl(fd,fcntl.F_SETFL,fl)
err=r.stderr.decode(errors='replace').replace('\r','\n')
print('sb rc', r.returncode, '| NAK/retries:', err.count('Retry'), '|', err.strip().splitlines()[-1])
if baud != 115200:
    print(until(b'ESC ...',10)); time.sleep(0.2); s.baudrate=115200; time.sleep(0.2); s.write(b'\x1b')
print(until(b'=> ',10))
print(f'transfer {dt:.1f} s, {len(data)/dt:.0f} B/s ({len(data)*10/dt/baud*100:.0f}% of line rate)')
send(f'md.l {addr:#x} {len(data)//4:#x}'); o=until(b'=> ', 300)
got=bytearray()
for m in re.finditer(r'^([0-9a-f]{8}): ((?:[0-9a-f]{8} ?){1,4})', o, re.M):
    got+=b''.join(bytes.fromhex(w) for w in m.group(2).split())
if not got: print('md output head:', repr(o[:300]))
print('read back', len(got), 'bytes;', 'MATCH' if bytes(got)==data else
      f'DIFFER at {next((i for i,(a,b) in enumerate(zip(got,data)) if a!=b), min(len(got),len(data))):#x}')
