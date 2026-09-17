import socket
import struct
from pathlib import Path
from typing import Iterable, Optional, Sequence, Union

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

MATRIX_PAIRS_INST = 11
MATRIX_PAIRS_FLAG_CLEAR_BETWEEN = 0x01
MATRIX_PAIRS_FLAG_CLEAR_AT_END = 0x02


_total_buffer = b""
_transport_handle = None


def _normalize_matrix_pairs(row_col_pairs):
    normalized = []
    for row, col in row_col_pairs:
        r = int(row)
        c = int(col)
        _validate_matrix_pair(r, c)
        normalized.append((r, c))
    return normalized


def _validate_column_row_mapping(pairs):
    col_to_row = {}
    for r, c in pairs:
        mapped_row = col_to_row.get(c)
        if mapped_row is not None and mapped_row != r:
            raise ValueError(
                f"invalid matrix mapping: column {c} is assigned to both row {mapped_row} and row {r}"
            )
        col_to_row[c] = r

__all__ = [
    "controller",
]


class controller:
    """Import-friendly client for DAC, matrix, mux, and network-config commands."""

    def __init__(
        self,
        transport: Optional[str] = None,
        ip: Optional[str] = None,
        port: Optional[int] = None,
        serial: Optional[str] = None,
        serial_port: Optional[str] = None,
        uart_port: Optional[str] = None,
        baud: Optional[int] = None,
        baud_rate: Optional[int] = None,
        uart_baudrate: int = 115200,
        uart_timeout_s: float = 1.0,
        auto_send: bool = True,
    ) -> None:
        transport_hint = transport
        if transport_hint not in (None, "ethernet", "uart", "serial"):
            if ip is None and serial is None and serial_port is None and uart_port is None:
                try:
                    _parse_ipv4(str(transport_hint))
                    ip = str(transport_hint)
                except (TypeError, ValueError):
                    serial_port = str(transport_hint)
                transport = None

        selected_uart_port = serial_port if serial_port is not None else uart_port
        if selected_uart_port is None:
            selected_uart_port = serial

        if baud_rate is not None:
            uart_baudrate = int(baud_rate)

        if baud is not None:
            uart_baudrate = int(baud)

        has_ethernet_args = (ip is not None) or (port is not None)
        has_uart_args = selected_uart_port is not None

        if transport is None:
            if has_ethernet_args and has_uart_args:
                raise ValueError("Pass either ethernet args (ip/port) or serial args (serial_port/uart_port), not both")
            resolved_transport = "uart" if has_uart_args else "ethernet"
        else:
            normalized = transport.strip().lower()
            if normalized == "serial":
                normalized = "uart"
            if normalized not in ("ethernet", "uart"):
                raise ValueError("transport must be 'ethernet', 'uart', or 'serial'")
            resolved_transport = normalized

        if resolved_transport == "ethernet" and has_uart_args:
            raise ValueError("serial_port/uart_port cannot be used when transport is ethernet")
        if resolved_transport == "uart" and has_ethernet_args:
            raise ValueError("ip/port cannot be used when transport is uart")

        self.transport = resolved_transport
        self.ip = ip if ip is not None else STM32_IP
        self.port = _validate_tcp_port(STM32_PORT if port is None else port)
        self.uart_port = selected_uart_port if selected_uart_port is not None else UART_PORT
        self.uart_baudrate = int(uart_baudrate)
        self.uart_timeout_s = float(uart_timeout_s)
        self.auto_send = bool(auto_send)
        self._buffer = b""
        self._transport_handle = None

    def _auto_send_if_enabled(self) -> None:
        if not self.auto_send:
            return

        if self._transport_handle is None:
            self.connect()

        self.send(command_file=None)

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

    def set_voltage(
        self,
        channel: Union[int, Iterable[Sequence[float]]],
        voltage: Optional[float] = None,
    ) -> None:
        if voltage is None:
            try:
                pairs = list(channel)  # type: ignore[arg-type]
            except TypeError as exc:
                raise TypeError(
                    "set_voltage expects either (channel, voltage) or an iterable of (channel, voltage) pairs"
                ) from exc

            for pair in pairs:
                if len(pair) != 2:
                    raise ValueError(f"invalid voltage pair {pair!r}: expected (channel, voltage)")

                ch = int(pair[0])
                volt = float(pair[1])
                _validate_channel(ch)
                self._buffer += struct.pack("<B B H", 1, ch, code(volt))

            self._auto_send_if_enabled()
            return

        ch = int(channel)
        _validate_channel(ch)
        self._buffer += struct.pack("<B B H", 1, ch, code(float(voltage)))
        self._auto_send_if_enabled()

    def config_matrix(
        self,
        row_col_pairs: Iterable[Sequence[int]],
        *,
        clear_between: bool = False,
        clear_at_end: bool = False,
    ) -> None:
        pairs = _normalize_matrix_pairs(row_col_pairs)
        _validate_column_row_mapping(pairs)

        if len(pairs) > 9:
            raise ValueError(f"too many matrix pairs: {len(pairs)} (max 9)")

        flags = 0
        if clear_between:
            flags |= MATRIX_PAIRS_FLAG_CLEAR_BETWEEN
        if clear_at_end:
            flags |= MATRIX_PAIRS_FLAG_CLEAR_AT_END

        payload = bytearray(struct.pack("<BBB", MATRIX_PAIRS_INST, flags, len(pairs)))
        for r, c in pairs:
            payload += struct.pack("<BB", r, c)

        self._buffer += bytes(payload)
        self._auto_send_if_enabled()

    def set_mux(self, mux_id: int) -> None:
        _validate_mux(int(mux_id))
        self._buffer += struct.pack("<BB", 9, int(mux_id))
        self._auto_send_if_enabled()

    def set_network_config(
        self,
        ip: Optional[str] = None,
        port: int = STM32_PORT,
        *,
        ip_address: Optional[str] = None,
    ) -> None:
        selected_ip = ip_address if ip_address is not None else ip
        if selected_ip is None:
            raise ValueError("set_network_config requires ip (or ip_address)")

        octets = _parse_ipv4(selected_ip)
        tcp_port = _validate_tcp_port(port)
        self._buffer += struct.pack("<BBBBBH", 10, octets[0], octets[1], octets[2], octets[3], tcp_port)
        self._auto_send_if_enabled()

    def send(self, command_file: Optional[str] = DEFAULT_COMMAND_FILE, append_end: bool = True) -> bytes:
        if self._transport_handle is None:
            raise RuntimeError(
                "Transport is not connected. Call connect() first, then queue commands, then call send()."
            )

        payload = self._buffer + (b"e" if append_end else b"")
        if command_file:
            Path(command_file).write_bytes(payload)

        if self.transport == "ethernet":
            try:
                self._transport_handle.sendall(payload)
            except OSError:
                # Peer may have closed between REPL commands; reconnect and retry once.
                self.close()
                self.connect()
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


def _validate_channel(channel):
    if channel < LOGICAL_CHANNEL_MIN or channel > LOGICAL_CHANNEL_MAX:
        raise ValueError(f"channel must be {LOGICAL_CHANNEL_MIN}-{LOGICAL_CHANNEL_MAX}, got {channel}")


def _validate_ltc2688_channel(channel):
    if channel < LTC2688_LOGICAL_MIN or channel > LTC2688_LOGICAL_MAX:
        raise ValueError(
            f"LTC2688 logical channel must be {LTC2688_LOGICAL_MIN}-{LTC2688_LOGICAL_MAX}, got {channel}"
        )


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







