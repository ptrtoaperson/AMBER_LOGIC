import argparse
import socket
import struct
from pathlib import Path
from typing import Iterable, Optional, Sequence

try:
    import serial  # type: ignore[import-not-found]
except ImportError:
    serial = None


# ========================
# Transport configuration
# ========================
TRANSPORT = "ethernet"  # "ethernet" or "uart"

# Ethernet settings
STM32_IP = "192.168.1.50"
STM32_PORT = 5000

# UART settings
UART_PORT = "/dev/ttyUSB0"
UART_BAUDRATE = 115200
UART_TIMEOUT_S = 1.0

# Command dump file
DEFAULT_COMMAND_FILE = "command.bin"

# Logical channel map in firmware:
# 0-15 -> LTC2688 channels 0-15
LOGICAL_CHANNEL_MIN = 0
LOGICAL_CHANNEL_MAX = 15
LTC2688_LOGICAL_MIN = 0
LTC2688_LOGICAL_MAX = 15
TGP_MIN = 0
TGP_MAX = 2
TRIG_MIN = 0
TRIG_MAX = 1
TRIG_V_MIN = -11.55
TRIG_V_MAX = 11.55
SEQ_TRIG_MIN = 0
SEQ_TRIG_MAX = 3
MATRIX_ROW_MIN = 1
MATRIX_ROW_MAX = 4
MATRIX_COL_MIN = 1
MATRIX_COL_MAX = 9
MUX_MIN = 1
MUX_MAX = 4
TCP_PORT_MIN = 1
TCP_PORT_MAX = 65535


_total_buffer = b""
_transport_handle = None


def _matrix_pairs_to_masks(row_col_pairs):
    row_mask = 0
    col_mask = 0
    used_cols = set()

    for row, col in row_col_pairs:
        r = int(row)
        c = int(col)
        _validate_matrix_pair(r, c)

        if c in used_cols:
            raise ValueError(f"duplicate column assignment is not allowed: col {c}")

        used_cols.add(c)

        row_mask |= (1 << (r - 1))
        col_mask |= (1 << (c - 1))
    return row_mask, col_mask

__all__ = [
    "AmberControllerClient",
    "SemiQonVsrcClient",
    "configure_transport",
    "reset_buffer",
    "code",
    "set_voltage",
    "config_matrix",
    "config_matrix_masks",
    "set_mux",
    "set_network_config",
    "connect_transport",
    "close_transport",
    "send",
    "linear_points",
    "queue_linear_channels",
    "run_default_demo",
    "main",
]


class SemiQonVsrcClient:
    """Import-friendly client for DAC, matrix, mux, and network-config commands."""

    def __init__(
        self,
        transport: str = "ethernet",
        ip: str = "192.168.1.50",
        port: int = 5000,
        uart_port: str = "/dev/ttyUSB0",
        uart_baudrate: int = 115200,
        uart_timeout_s: float = 1.0,
    ) -> None:
        self.transport = transport
        self.ip = ip
        self.port = int(port)
        self.uart_port = uart_port
        self.uart_baudrate = int(uart_baudrate)
        self.uart_timeout_s = float(uart_timeout_s)
        self._buffer = b""
        self._transport_handle = None

    def reset_buffer(self) -> None:
        self._buffer = b""

    def connect(self) -> None:
        if self.transport == "ethernet":
            connected = False
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(1.0)

            while not connected:
                try:
                    s.connect((self.ip, self.port))
                    s.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
                    connected = True
                    s.settimeout(None)
                except (socket.timeout, ConnectionRefusedError, OSError):
                    s.close()
                    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    s.settimeout(0.04)

            self._transport_handle = s
            return

        if self.transport == "uart":
            if serial is None:
                raise ImportError("pyserial is required for UART transport. Install with: pip install pyserial")

            self._transport_handle = serial.Serial(
                port=self.uart_port,
                baudrate=self.uart_baudrate,
                timeout=self.uart_timeout_s,
                write_timeout=self.uart_timeout_s,
            )
            return

        raise ValueError(f"Unsupported transport: {self.transport}")

    def close(self) -> None:
        if self._transport_handle is None:
            return
        self._transport_handle.close()
        self._transport_handle = None

    def set_voltage(self, channel: int, voltage: float) -> None:
        _validate_channel(channel)
        self._buffer += struct.pack("<B B H", 1, int(channel), code(float(voltage)))

    def config_matrix_masks(self, row_mask: int, col_mask: int) -> None:
        self._buffer += struct.pack("<BHH", 8, int(row_mask) & 0xFFFF, int(col_mask) & 0xFFFF)

    def config_matrix(self, row_col_pairs: Iterable[Sequence[int]]) -> None:
        row_mask, col_mask = _matrix_pairs_to_masks(row_col_pairs)
        self.config_matrix_masks(row_mask, col_mask)

    def set_mux(self, mux_id: int) -> None:
        _validate_mux(int(mux_id))
        self._buffer += struct.pack("<BB", 9, int(mux_id))

    def set_network_config(self, ip: str, port: int) -> None:
        octets = _parse_ipv4(ip)
        tcp_port = _validate_tcp_port(port)
        self._buffer += struct.pack("<BBBBBH", 10, octets[0], octets[1], octets[2], octets[3], tcp_port)

    def send(self, command_file: Optional[str] = DEFAULT_COMMAND_FILE, append_end: bool = True) -> bytes:
        if self._transport_handle is None:
            raise RuntimeError("Transport is not connected. Call connect() first.")

        payload = self._buffer + (b"e" if append_end else b"")
        if command_file:
            Path(command_file).write_bytes(payload)

        if self.transport == "ethernet":
            self._transport_handle.sendall(payload)
        elif self.transport == "uart":
            self._transport_handle.write(payload)
            self._transport_handle.flush()
        else:
            raise ValueError(f"Unsupported transport: {self.transport}")

        self._buffer = b""
        return payload


def configure_transport(
    transport=None,
    ip=None,
    port=None,
    uart_port=None,
    uart_baudrate=None,
    uart_timeout_s=None,
):
    """Update module transport configuration at runtime."""
    global TRANSPORT, STM32_IP, STM32_PORT, UART_PORT, UART_BAUDRATE, UART_TIMEOUT_S

    if transport is not None:
        if transport not in ("ethernet", "uart"):
            raise ValueError("transport must be 'ethernet' or 'uart'")
        TRANSPORT = transport

    if ip is not None:
        STM32_IP = ip
    if port is not None:
        STM32_PORT = int(port)
    if uart_port is not None:
        UART_PORT = uart_port
    if uart_baudrate is not None:
        UART_BAUDRATE = int(uart_baudrate)
    if uart_timeout_s is not None:
        UART_TIMEOUT_S = float(uart_timeout_s)


def reset_buffer():
    """Clear the queued command buffer."""
    global _total_buffer
    _total_buffer = b""


def _validate_channel(channel):
    if channel < LOGICAL_CHANNEL_MIN or channel > LOGICAL_CHANNEL_MAX:
        raise ValueError(f"channel must be {LOGICAL_CHANNEL_MIN}-{LOGICAL_CHANNEL_MAX}, got {channel}")


def _validate_tgp(tgp_id):
    if tgp_id < TGP_MIN or tgp_id > TGP_MAX:
        raise ValueError(f"tgp_id must be {TGP_MIN}-{TGP_MAX}, got {tgp_id}")


def _validate_pwm(channel, tgp_id, freq):
    _validate_channel(channel)
    _validate_tgp(tgp_id)
    if freq <= 0.0:
        raise ValueError(f"freq must be > 0, got {freq}")


def _validate_ltc2688_channel(channel):
    if channel < LTC2688_LOGICAL_MIN or channel > LTC2688_LOGICAL_MAX:
        raise ValueError(
            f"LTC2688 logical channel must be {LTC2688_LOGICAL_MIN}-{LTC2688_LOGICAL_MAX}, got {channel}"
        )


def _validate_trig_channel(trig_id):
    if trig_id < TRIG_MIN or trig_id > TRIG_MAX:
        raise ValueError(f"trig_id must be {TRIG_MIN} or {TRIG_MAX}, got {trig_id}")


def _validate_trig_voltage(v_trig):
    if v_trig < TRIG_V_MIN or v_trig > TRIG_V_MAX:
        raise ValueError(f"trigger voltage must be in [{TRIG_V_MIN}, {TRIG_V_MAX}], got {v_trig}")


def _validate_sequence_trigger_pin(trigger_pin):
    if trigger_pin < SEQ_TRIG_MIN or trigger_pin > SEQ_TRIG_MAX:
        raise ValueError(f"trigger_pin must be {SEQ_TRIG_MIN}-{SEQ_TRIG_MAX}, got {trigger_pin}")


def _validate_matrix_pair(row, col):
    if row < MATRIX_ROW_MIN or row > MATRIX_ROW_MAX:
        raise ValueError(f"row must be {MATRIX_ROW_MIN}-{MATRIX_ROW_MAX}, got {row}")
    if col < MATRIX_COL_MIN or col > MATRIX_COL_MAX:
        raise ValueError(f"col must be {MATRIX_COL_MIN}-{MATRIX_COL_MAX}, got {col}")


def _validate_mux(mux_id):
    if mux_id < MUX_MIN or mux_id > MUX_MAX:
        raise ValueError(f"mux_id must be {MUX_MIN}-{MUX_MAX}, got {mux_id}")


def _parse_ipv4(ip):
    if isinstance(ip, str):
        parts = ip.strip().split(".")
    else:
        parts = list(ip)

    if len(parts) != 4:
        raise ValueError(f"ip must have 4 octets, got {ip}")

    octets = []
    for p in parts:
        v = int(p)
        if v < 0 or v > 255:
            raise ValueError(f"invalid IPv4 octet {v}, expected 0-255")
        octets.append(v)

    return octets


def _validate_tcp_port(port):
    p = int(port)
    if p < TCP_PORT_MIN or p > TCP_PORT_MAX:
        raise ValueError(f"port must be {TCP_PORT_MIN}-{TCP_PORT_MAX}, got {p}")
    return p


def code(volt):
    """Convert volts in [-10, 10] to 16-bit DAC code."""
    if volt > 10.0:
        volt = 9.999999
    if volt < -10.0:
        volt = -9.999999

    code_val = int(((volt + 10.0) / 20.0) * 65535)
    if code_val > 0xFFFF:
        code_val = 0xFFFF
    return code_val


def Vsrc(command):
    """Queue voltage-write commands. command is iterable of (channel, code)."""
    global _total_buffer

    for cmd_tuple in command:
        packed = struct.pack("<B B H", 1, *cmd_tuple)
        _total_buffer += packed


def set_voltage(channel, voltage):
    """Queue one voltage setpoint in physical volts."""
    _validate_channel(channel)
    Vsrc([(channel, code(voltage))])


def start_PWM(channel, id, low, high, freq):
    raise NotImplementedError("PWM/toggle commands are disabled in DAC+matrix+mux profile")


def stop_PWM(channel, id):
    raise NotImplementedError("PWM/toggle commands are disabled in DAC+matrix+mux profile")


def start_ltc2688_pwm(channel, tgp_id, low, high, freq):
    """Start hardware PWM on LTC2688 logical channel (0-15)."""
    _validate_ltc2688_channel(channel)
    start_PWM(channel=channel, id=tgp_id, low=low, high=high, freq=freq)


def assign_ltc2688_pwm(assignments):
    """Bulk-assign PWM for LTC2688 channels.

    Each item in assignments must be:
    (channel_0_to_15, tgp_id_0_to_2, low_volt, high_volt, freq_hz)
    """
    for channel, tgp_id, low, high, freq in assignments:
        start_ltc2688_pwm(channel, tgp_id, low, high, freq)


def config_matrix(row_col_pairs):
    """Set matrix row/col outputs using 1-based indices.

    row_col_pairs is iterable of (row, col), where row is 1..4 and col is 1..9.
    Any mentioned row/col is driven high;
    all unmentioned rows/cols are driven low.
    """
    global _total_buffer

    row_mask, col_mask = _matrix_pairs_to_masks(row_col_pairs)

    packed = struct.pack("<BHH", 8, row_mask, col_mask)
    _total_buffer += packed


def config_matrix_masks(row_mask, col_mask):
    """Queue matrix command using raw bitmasks.

    This bypasses row/col index validation and sends masks exactly as provided.
    """
    global _total_buffer

    row_mask = int(row_mask) & 0xFFFF
    col_mask = int(col_mask) & 0xFFFF

    packed = struct.pack("<BHH", 8, row_mask, col_mask)
    _total_buffer += packed


def set_mux(mux_id):
    """Select one mux line high (1..4), all others low in firmware."""
    global _total_buffer
    _validate_mux(int(mux_id))
    packed = struct.pack("<BB", 9, int(mux_id))
    _total_buffer += packed


def set_network_config(ip, port):
    """Queue persistent network configuration write (flash)."""
    global _total_buffer

    octets = _parse_ipv4(ip)
    tcp_port = _validate_tcp_port(port)
    packed = struct.pack("<BBBBBH", 10, octets[0], octets[1], octets[2], octets[3], tcp_port)
    _total_buffer += packed


def send_voltage_sequence(dac_channel, trigger_pin, edge, voltages):
    raise NotImplementedError("Triggered sequence commands are disabled in DAC+matrix+mux profile")


def send_soft_sequence(dac_channel, delay_ms, voltages):
    raise NotImplementedError("Software sequence commands are disabled in DAC+matrix+mux profile")


def set_trigger_level(trig_id, v_trig):
    raise NotImplementedError("Trigger DAC commands are disabled in DAC+matrix+mux profile")


def connect_transport():
    """Open selected transport and keep handle globally for send()."""
    global _transport_handle

    if TRANSPORT == "ethernet":
        connected = False
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(1.0)

        while not connected:
            try:
                print(f"Attempting to connect to {STM32_IP}:{STM32_PORT}...")
                s.connect((STM32_IP, STM32_PORT))

                s.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
                if hasattr(socket, "TCP_KEEPIDLE"):
                    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE, 30)
                elif hasattr(socket, "SIO_KEEPALIVE_VALS"):
                    s.ioctl(socket.SIO_KEEPALIVE_VALS, (1, 30000, 5000))

                connected = True
                s.settimeout(None)
                print("Successfully connected over Ethernet")
            except (socket.timeout, ConnectionRefusedError, OSError) as e:
                print(f"Connection failed: {e}. Retrying...")
                s.close()
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.settimeout(0.04)

        _transport_handle = s
        return

    if TRANSPORT == "uart":
        if serial is None:
            raise ImportError("pyserial is required for UART transport. Install with: pip install pyserial")

        print(f"Opening UART {UART_PORT} @ {UART_BAUDRATE}...")
        _transport_handle = serial.Serial(
            port=UART_PORT,
            baudrate=UART_BAUDRATE,
            timeout=UART_TIMEOUT_S,
            write_timeout=UART_TIMEOUT_S,
        )
        print("UART opened")
        return

    raise ValueError(f"Unsupported transport: {TRANSPORT}")


def close_transport():
    global _transport_handle

    if _transport_handle is None:
        return

    _transport_handle.close()
    _transport_handle = None


def send(command_file=DEFAULT_COMMAND_FILE, append_end=True, echo_bytes=False):
    """Transmit queued buffer through active transport.

    command_file: optional path to write raw payload for debugging.
    append_end: append protocol end marker 'e'.
    echo_bytes: print raw bytes payload.
    """
    global _total_buffer

    if _transport_handle is None:
        raise RuntimeError("Transport is not connected. Call connect_transport() first.")

    payload = _total_buffer
    if append_end:
        payload += b"e"

    if command_file:
        Path(command_file).write_bytes(payload)

    if TRANSPORT == "ethernet":
        _transport_handle.sendall(payload)
    elif TRANSPORT == "uart":
        _transport_handle.write(payload)
        _transport_handle.flush()
    else:
        raise ValueError(f"Unsupported transport: {TRANSPORT}")

    if echo_bytes:
        print(payload)

    _total_buffer = b""


def linear_points(start, stop, count):
    """Return count evenly spaced floats from start to stop (inclusive)."""
    if count < 2:
        return [float(start)]
    step = (stop - start) / float(count - 1)
    return [float(start + i * step) for i in range(count)]


def queue_linear_channels(channels=16, start=-10.0, stop=10.0):
    """Queue a linear ramp across channels [0, channels-1]."""
    if channels < 1 or channels > (LOGICAL_CHANNEL_MAX + 1):
        raise ValueError(f"channels must be 1-{LOGICAL_CHANNEL_MAX + 1}, got {channels}")

    values = linear_points(start, stop, channels)
    for ch, v in enumerate(values):
        Vsrc([(ch, code(v))])


def run_default_demo():
    """Equivalent to the old script behavior: set all channels linearly and send."""
    connect_transport()
    try:
        queue_linear_channels(channels=16, start=-10.0, stop=10.0)
        send(command_file=DEFAULT_COMMAND_FILE, append_end=True, echo_bytes=False)
    finally:
        close_transport()


def _build_parser():
    parser = argparse.ArgumentParser(
        prog="amber_controller",
        description="SemiQon voltage-source command builder and transport CLI",
    )

    parser.add_argument("--transport", choices=["ethernet", "uart"], default=TRANSPORT)
    parser.add_argument("--ip", default=STM32_IP)
    parser.add_argument("--port", type=int, default=STM32_PORT)
    parser.add_argument("--uart-port", default=UART_PORT)
    parser.add_argument("--baudrate", type=int, default=UART_BAUDRATE)
    parser.add_argument("--uart-timeout", type=float, default=UART_TIMEOUT_S)
    parser.add_argument("--command-file", default=DEFAULT_COMMAND_FILE)
    parser.add_argument("--echo-bytes", action="store_true")

    subparsers = parser.add_subparsers(dest="cmd")

    linear = subparsers.add_parser("linear", help="Set channels to a linear ramp")
    linear.add_argument("--channels", type=int, default=16)
    linear.add_argument("--start", type=float, default=-10.0)
    linear.add_argument("--stop", type=float, default=10.0)

    setp = subparsers.add_parser("set", help="Set explicit channel:voltage pairs")
    setp.add_argument("pairs", nargs="+", help="Pairs in form channel:voltage (example 0:-1.2 8:2.5)")

    netp = subparsers.add_parser("net", help="Persist IPv4 and TCP port to flash")
    netp.add_argument("ip", help="IPv4 address, example 192.168.1.50")
    netp.add_argument("port", type=int, help="TCP port, 1-65535")

    subparsers.add_parser("demo", help="Run default demo (same as old script behavior)")

    return parser


def _queue_pairs(pairs):
    for item in pairs:
        if ":" not in item:
            raise ValueError(f"Invalid pair '{item}'. Expected channel:voltage")
        ch_str, v_str = item.split(":", 1)
        ch = int(ch_str)
        v = float(v_str)
        set_voltage(ch, v)


def main(argv=None):
    parser = _build_parser()
    args = parser.parse_args(argv)

    configure_transport(
        transport=args.transport,
        ip=args.ip,
        port=args.port,
        uart_port=args.uart_port,
        uart_baudrate=args.baudrate,
        uart_timeout_s=args.uart_timeout,
    )

    cmd = args.cmd or "demo"

    connect_transport()
    try:
        reset_buffer()

        if cmd == "demo":
            queue_linear_channels(channels=16, start=-10.0, stop=10.0)
        elif cmd == "linear":
            queue_linear_channels(channels=args.channels, start=args.start, stop=args.stop)
        elif cmd == "set":
            _queue_pairs(args.pairs)
        elif cmd == "net":
            set_network_config(args.ip, args.port)
        else:
            parser.error(f"Unsupported command: {cmd}")

        send(command_file=args.command_file, append_end=True, echo_bytes=args.echo_bytes)
    finally:
        close_transport()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())


AmberControllerClient = SemiQonVsrcClient
