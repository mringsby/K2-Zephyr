#!/usr/bin/env python3
"""
Simple UDP test client for STM32 K2-Zephyr application
Sends structured packets with proper CRC32 validation
"""

import socket
import struct
import binascii
import time

# Configuration
TARGET_IP = "192.168.1.100"
TARGET_PORT = 12345

# CRC32 calculation (IEEE 802.3 polynomial)
def calculate_crc32(data):
    """Calculate CRC32 using IEEE 802.3 polynomial"""
    crc = 0xFFFFFFFF
    polynomial = 0xEDB88320
    
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ polynomial
            else:
                crc = crc >> 1
    
    return (~crc) & 0xFFFFFFFF

def create_packet(sequence, payload, corrupt_crc=False):
    """Create a structured packet with CRC32"""
    # Pack sequence (uint32) and payload (uint64) in network byte order (big-endian)
    seq_bytes = struct.pack('>I', sequence)      # >I = big-endian uint32
    payload_bytes = struct.pack('>Q', payload)   # >Q = big-endian uint64
    
    # Calculate CRC32 over sequence + payload
    data_for_crc = seq_bytes + payload_bytes
    crc32 = calculate_crc32(data_for_crc)
    
    # Corrupt CRC32 if requested (for testing error handling)
    if corrupt_crc:
        crc32 = crc32 ^ 0xDEADBEEF  # XOR with a value to corrupt it
        print(f"  ⚠️ CRC32 INTENTIONALLY CORRUPTED for testing!")
    
    crc_bytes = struct.pack('>I', crc32)         # >I = big-endian uint32
    
    # Complete packet
    packet = seq_bytes + payload_bytes + crc_bytes
    
    print(f"  Packet created:")
    print(f"    Sequence: {sequence}")
    print(f"    Payload: 0x{payload:016X}")
    print(f"    CRC32: 0x{crc32:08X}")
    print(f"    Size: {len(packet)} bytes")
    print(f"    Raw bytes: {binascii.hexlify(packet).decode().upper()}")
    
    return packet

def main():
    print("=== UDP Test Client for STM32 K2-Zephyr ===")
    print(f"Target: {TARGET_IP}:{TARGET_PORT}")
    print("Expected packet structure: [uint32 seq][uint64 payload][uint32 crc32] = 16 bytes")
    print()
    
    try:
        # Create UDP socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.settimeout(5.0)  # 5 second timeout
        
        print(f"Connected to {TARGET_IP}:{TARGET_PORT}")
        print()
        
        # Test cases with simple values
        test_cases = [
            {"seq": 1, "payload": 0x1234567890ABCDEF, "corrupt_crc": False},
            {"seq": 2, "payload": 0xDEADBEEFCAFEBABE, "corrupt_crc": False},
            {"seq": 3, "payload": 0x0000000000000000, "corrupt_crc": False},
            {"seq": 4, "payload": 0xFFFFFFFFFFFFFFFF, "corrupt_crc": False},
            {"seq": 100, "payload": 0x123456789ABCDEF0, "corrupt_crc": False},
            {"seq": 255, "payload": 0x5555AAAA5555AAAA, "corrupt_crc": True},
            {"seq": 254, "payload": 0x5555AAAA5555AAAA, "corrupt_crc": True},
            {"seq": 252, "payload": 0x5555AAAA5555AAAA, "corrupt_crc": False}
        ]
        
        for i, test in enumerate(test_cases, 1):
            print(f"--- Test Packet {i} ---")
            packet = create_packet(test["seq"], test["payload"], test["corrupt_crc"])
            
            try:
                bytes_sent = sock.sendto(packet, (TARGET_IP, TARGET_PORT))
                print(f"  ✓ Sent {bytes_sent} bytes successfully")
            except socket.error as e:
                print(f"  ✗ Error sending packet: {e}")
            
            print("  Waiting for STM32 to process...")
            time.sleep(2)
            print()
        
        print("=== Test Completed ===")
        print("Check your STM32 serial console for received packet logs.")
        
    except socket.error as e:
        print(f"Socket error: {e}")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        try:
            sock.close()
            print("UDP socket closed.")
        except:
            pass
    
    print("\nPress Enter to exit...")
    input()

if __name__ == "__main__":
    main()
