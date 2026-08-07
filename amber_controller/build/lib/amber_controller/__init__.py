"""SemiQon voltage-source Python package."""

from .client import *


class controller:
	"""Small convenience API for Ethernet or serial control.

	Examples:
		dev1 = controller(ip="192.168.1.50", port=5000)
		dev2 = controller(serial="/dev/ttyUSB0", baud=115200)
	"""

	def __init__(
		self,
		ip=None,
		port=5000,
		serial=None,
		baud=115200,
		timeout=1.0,
	):
		if ip is not None and serial is not None:
			raise ValueError("Use either Ethernet (ip/port) or serial (serial/baud), not both")

		if serial is not None:
			self._client = SemiQonVsrcClient(
				transport="uart",
				uart_port=str(serial),
				uart_baudrate=int(baud),
				uart_timeout_s=float(timeout),
			)
		else:
			self._client = SemiQonVsrcClient(
				transport="ethernet",
				ip=str(ip) if ip is not None else "192.168.1.50",
				port=int(port),
			)
		self._connected = False

	def _ensure_connected(self):
		if not self._connected:
			self._client.connect()
			self._connected = True

	def _send_current_buffer(self):
		# Firmware closes TCP after each packet, so Ethernet must reconnect per command.
		self._ensure_connected()
		try:
			self._client.send(command_file=None)
		except OSError:
			self._client.close()
			self._connected = False
			self._ensure_connected()
			self._client.send(command_file=None)
		finally:
			if self._client.transport == "ethernet":
				self._client.close()
				self._connected = False

	def close(self):
		self._client.close()
		self._connected = False

	def voltage(self, channel, value):
		self._client.reset_buffer()
		self._client.set_voltage(int(channel), float(value))
		self._send_current_buffer()

	def matrix(self, row, col):
		self._client.reset_buffer()
		self._client.config_matrix([(int(row), int(col))])
		self._send_current_buffer()

	def mux(self, mux_id):
		self._client.reset_buffer()
		self._client.set_mux(int(mux_id))
		self._send_current_buffer()


__all__ = [name for name in globals() if not name.startswith("_")]
