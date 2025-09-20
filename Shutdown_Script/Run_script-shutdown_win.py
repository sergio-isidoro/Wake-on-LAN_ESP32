#!/usr/bin/env python3
"""
Shutdown Listener (Hidden)
- Listens for a special UDP shutdown packet.
- Runs hidden in background when packaged as .exe with --noconsole.
- Logs events to C:\ShutdownListener\listener.log
"""

import socket
import os
import sys
import logging
import threading
from getmac import get_mac_address

# --- CONFIG ---
UDP_PORT = 9
SHUTDOWN_PREFIX = b'\xee\xee\xee\xee\xee\xee'
PACKET_SIZE = 102
LOG_FOLDER = r"C:\ShutdownListener"
LOG_FILE = os.path.join(LOG_FOLDER, "listener.log")

# Ensure log folder exists
os.makedirs(LOG_FOLDER, exist_ok=True)

# Logging (to file only, hidden)
logging.basicConfig(
    filename=LOG_FILE,
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)

# Optional console handler for debugging
console_handler = logging.StreamHandler()
console_handler.setFormatter(logging.Formatter('%(asctime)s - %(levelname)s - %(message)s'))

# For stopping listener
stop_event = threading.Event()

def get_local_mac_bytes():
    mac_str = get_mac_address()
    if not mac_str:
        raise RuntimeError("Could not obtain local MAC address.")
    return bytes.fromhex(mac_str.replace(':', '').replace('-', ''))

class UDPShutdownListener:
    def __init__(self, udp_port=UDP_PORT, simulate=False):
        self.udp_port = udp_port
        self.simulate = simulate
        self.sock = None
        self.local_mac = get_local_mac_bytes()
        logging.info("Local MAC: %s", get_mac_address())

    def start(self):
        shutdown_command = "shutdown /s /t 1" if os.name == 'nt' else "sudo /sbin/shutdown -h now"
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.sock.bind(('0.0.0.0', self.udp_port))
            self.sock.settimeout(1.0)
            logging.info("UDP listener started on port %d", self.udp_port)
        except Exception:
            logging.exception("Failed to start UDP listener")
            return

        try:
            while not stop_event.is_set():
                try:
                    data, addr = self.sock.recvfrom(4096)
                except socket.timeout:
                    continue
                except OSError:
                    break

                if len(data) != PACKET_SIZE or not data.startswith(SHUTDOWN_PREFIX):
                    continue

                packet_mac = data[6:12]
                if packet_mac != self.local_mac:
                    logging.info("Ignored packet from %s MAC mismatch", addr[0])
                    continue

                logging.info("Valid shutdown packet received from %s", addr[0])
                if self.simulate:
                    logging.info("SIMULATE mode: shutdown skipped")
                else:
                    logging.info("Executing shutdown command")
                    os.system(shutdown_command)

                # After valid packet, stop listener (optional)
                break

        except Exception:
            logging.exception("Error in listener loop")
        finally:
            self._cleanup()
            logging.info("Listener stopped")

    def _cleanup(self):
        try:
            if self.sock:
                self.sock.close()
        except Exception:
            pass

def main():
    simulate = '--simulate' in sys.argv
    listener = UDPShutdownListener(simulate=simulate)
    listener.start()

if __name__ == "__main__":
    main()
