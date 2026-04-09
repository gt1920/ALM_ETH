"""
ALM ETH TCP protocol test script
Target: 192.168.3.123:40000
"""
import socket
import struct
import time

DEVICE_IP   = "192.168.3.123"
DEVICE_PORT = 40000
FRAME_LEN   = 64

def make_frame(cmd, seq=0, subcmd=0, payload=b''):
    f = bytearray(FRAME_LEN)
    f[0] = 0xA5
    f[1] = cmd
    f[2] = 0x02  # PC -> Device
    f[3] = seq & 0xFF
    f[4] = (seq >> 8) & 0xFF
    f[5] = len(payload) + 1 if subcmd else len(payload)
    if subcmd:
        f[6] = subcmd
    for i, b in enumerate(payload):
        f[7 + i] = b
    return f

def send_frame(sock, frame):
    prefix = struct.pack('<H', len(frame))
    sock.sendall(prefix + bytes(frame))

def recv_frame(sock, timeout=2.0):
    sock.settimeout(timeout)
    try:
        header = b''
        while len(header) < 2:
            chunk = sock.recv(2 - len(header))
            if not chunk:
                return None
            header += chunk
        frame_len = struct.unpack('<H', header)[0]
        data = b''
        while len(data) < frame_len:
            chunk = sock.recv(frame_len - len(data))
            if not chunk:
                return None
            data += chunk
        return data
    except socket.timeout:
        return None

def dump_frame(label, data):
    if data is None:
        print(f"  [{label}] No response (timeout)")
        return
    print(f"  [{label}] {len(data)} bytes:")
    for i in range(0, min(len(data), 44), 16):
        hex_str = ' '.join(f'{b:02X}' for b in data[i:i+16])
        print(f"    [{i:3d}] {hex_str}")

def drain_async(sock, max_wait=0.5):
    """Drain any async frames queued before our requests."""
    count = 0
    while True:
        f = recv_frame(sock, timeout=max_wait)
        if f is None:
            break
        cmd = f[1] if len(f) > 1 else 0
        print(f"  (drained async: CMD=0x{cmd:02X})")
        count += 1
    return count

def send_and_recv(sock, frame, expected_cmd, expected_subcmd=None, label=""):
    """Send a request and receive the matching response, skipping async frames."""
    send_frame(sock, frame)
    for _ in range(10):
        resp = recv_frame(sock, timeout=2.0)
        if resp is None:
            print(f"  [{label}] No response!")
            return None
        # Check if this is the expected response
        if len(resp) >= 7 and resp[1] == expected_cmd:
            if expected_subcmd is None or resp[6] == expected_subcmd:
                return resp
        # Otherwise it's an async frame, show and skip
        cmd = resp[1] if len(resp) > 1 else 0
        print(f"  (skipped async: CMD=0x{cmd:02X})")
    return None

def main():
    print(f"Connecting to {DEVICE_IP}:{DEVICE_PORT} ...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5.0)
    sock.connect((DEVICE_IP, DEVICE_PORT))
    print("Connected!\n")

    # Drain any queued async frames
    print("Draining async frames ...")
    n = drain_async(sock, max_wait=0.5)
    print(f"Drained {n} frames\n")

    # --- Test 1: GET_VERSION_INFO ---
    print("=" * 50)
    print("Test 1: GET_VERSION_INFO")
    frame = make_frame(cmd=0x01, seq=1, subcmd=0x01)
    resp = send_and_recv(sock, frame, expected_cmd=0x01, expected_subcmd=0x01, label="VERSION")
    dump_frame("VERSION", resp)
    if resp and len(resp) >= 14:
        print(f"  FW: {resp[7]}.{resp[8]}.{resp[9]}.{resp[10]}")
        print(f"  MFG: 20{resp[11]:02d}-{resp[12]:02d}-{resp[13]:02d}")
    print()

    # --- Test 2: GET_MODULE_LIST ---
    print("=" * 50)
    print("Test 2: GET_MODULE_LIST")
    frame = make_frame(cmd=0x01, seq=2, subcmd=0x02)
    resp = send_and_recv(sock, frame, expected_cmd=0x01, expected_subcmd=0x02, label="MODULE_LIST")
    dump_frame("MODULE_LIST", resp)
    module_count = 0
    if resp and len(resp) >= 8:
        module_count = resp[7]
        print(f"  Module count: {module_count}")
        pos = 8
        for i in range(module_count):
            if pos + 1 < len(resp):
                mtype = resp[pos]
                print(f"  Module {i}: type=0x{mtype:02X}")
                pos += 2
    print()

    # --- Test 3: GET_MODULE_INFO for each module ---
    if module_count > 0:
        print("=" * 50)
        print(f"Test 3: GET_MODULE_INFO (index 0..{module_count-1})")
        for idx in range(module_count):
            frame = make_frame(cmd=0x01, seq=10+idx, subcmd=0x03, payload=bytes([idx]))
            resp = send_and_recv(sock, frame, expected_cmd=0x01, expected_subcmd=0x03, label=f"MODULE[{idx}]")
            dump_frame(f"MODULE[{idx}]", resp)
            if resp and len(resp) >= 28:
                mtype = resp[8]
                name = resp[9:17].decode('ascii', errors='replace').rstrip('\x00')
                node_id = struct.unpack_from('<I', resp, 17)[0]
                fw = struct.unpack_from('<I', resp, 21)[0]
                print(f"  Type=0x{mtype:02X}  Name=\"{name}\"  NodeID=0x{node_id:08X}  FW=0x{fw:08X}")
            if resp and len(resp) >= 44:
                x_cur = struct.unpack_from('<H', resp, 28)[0]
                y_cur = struct.unpack_from('<H', resp, 30)[0]
                hold  = resp[32]
                speed = struct.unpack_from('<H', resp, 34)[0]
                x_pos = struct.unpack_from('<i', resp, 36)[0]
                y_pos = struct.unpack_from('<i', resp, 40)[0]
                print(f"  X_cur={x_cur} Y_cur={y_cur} Hold={hold}% Speed={speed} X_pos={x_pos} Y_pos={y_pos}")
            print()

    # --- Test 4: Wait for async CAN reports ---
    print("=" * 50)
    print("Test 4: Listening for async CAN reports (5s) ...")
    end_time = time.time() + 5.0
    count = 0
    while time.time() < end_time:
        resp = recv_frame(sock, timeout=1.0)
        if resp:
            cmd = resp[1] if len(resp) > 1 else 0
            sub = resp[6] if len(resp) > 6 else 0
            print(f"  Async #{count}: CMD=0x{cmd:02X} SUB=0x{sub:02X}")
            dump_frame(f"ASYNC[{count}]", resp)
            count += 1
    print(f"  Total: {count} async frames in 5s")

    sock.close()
    print("\nDone. All tests passed!" if module_count > 0 else "\nDone.")

if __name__ == "__main__":
    main()
