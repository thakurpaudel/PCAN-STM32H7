#!/usr/bin/env python3
import can
import time
import sys

"""
PCAN-USB Pro FD Verification Script
Tests sequential communication between CAN0 and CAN1.
"""

def test_pcan_communication():
    print("=== PCAN-USB Pro FD Communication Test ===")


    
    # 1. Setup Interfaces
    print("\n[Setup] Opening interfaces...")
    try:
        # Assuming 500k bitrate, standard CAN
        bus0 = can.Bus(interface='socketcan', channel='can0')
        bus1 = can.Bus(interface='socketcan', channel='can1')
        print("  -> can0 and can1 opened successfully.")
    except OSError as e:
        print(f"  [ERROR] Could not open CAN interfaces: {e}")
        print("  Hint: Ensure interfaces are up: 'sudo ip link set can0 up type can bitrate 500000'")
        sys.exit(1)

    try:

        # ---------------------------------------------------------
        # Test 1: CAN0 -> CAN1
        # ---------------------------------------------------------
        print("\n--- Test 1: Sending from CAN0 -> Receiving on CAN1 ---")
        
        # Create message
        msg_out = can.Message(arbitration_id=0x100, data=[0x01, 0x02, 0x03, 0x04], is_extended_id=False)
        
        # Send
        print(f"  [TX] can0 sends ID=0x{msg_out.arbitration_id:X}")
        bus0.send(msg_out)
        
        # Receive
        print("  [RX] can1 waiting...")
        msg_in = bus1.recv(timeout=1.0)
        
        if msg_in:
            print(f"  [RX] can1 received: {msg_in}")
            if msg_in.arbitration_id == 0x100 and list(msg_in.data) == [0x01, 0x02, 0x03, 0x04]:
                print("  -> PASS: Data match.")
            else:
                print("  -> FAIL: Data mismatch.")
        else:
            print("  -> FAIL: Timeout (No message received).")

        time.sleep(0.5)

        # ---------------------------------------------------------
        # Test 2: CAN1 -> CAN0
        # ---------------------------------------------------------
        print("\n--- Test 2: Sending from CAN1 -> Receiving on CAN0 ---")
        
        # Create message
        msg_out = can.Message(arbitration_id=0x200, data=[0xA0, 0xB0, 0xC0, 0xD0], is_extended_id=False)
        
        # Send
        print(f"  [TX] can1 sends ID=0x{msg_out.arbitration_id:X}")
        bus1.send(msg_out)
        
        # Receive
        print("  [RX] can0 waiting...")
        msg_in = bus0.recv(timeout=1.0)
        
        if msg_in:
            print(f"  [RX] can0 received: {msg_in}")
            if msg_in.arbitration_id == 0x200 and list(msg_in.data) == [0xA0, 0xB0, 0xC0, 0xD0]:
                print("  -> PASS: Data match.")
            else:
                print("  -> FAIL: Data mismatch.")
        else:
            print("  -> FAIL: Timeout (No message received).")
            
    except can.CanError as e:
        print(f"\n[ERROR] CAN Warning/Error: {e}")
    except KeyboardInterrupt:
        print("\n[Aborted]")
    finally:
        bus0.shutdown()
        bus1.shutdown()
        print("\n[Cleanup] Interfaces closed.")

if __name__ == "__main__":
    test_pcan_communication()
