import serial
import time
import math

# --- CONFIGURATION ---
SERIAL_PORT = 'COM3' 
BAUD_RATE = 115200
OUTPUT_FILE = 'tmag5170_data.csv'

try:
    print(f"Connecting to {SERIAL_PORT}...")
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1) 
    ser.dtr = True
    ser.rts = True
    time.sleep(1)

    ser.reset_input_buffer()
    ser.reset_output_buffer()

    print("Waiting for Pico to respond to trigger...")
    
    with open(OUTPUT_FILE, 'w') as file:
        file.write("Time(ms),X(mT),Y(mT),Z(mT),XY_Angle(deg),XZ_Angle(deg)\n")
        
        is_recording = False
        while True:
            if not is_recording:
                ser.write(b'r\n')
                ser.flush()
            
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            
            if line:
                if not is_recording:
                    # ONLY start recording if we see the Pico's actual data header
                    if line.startswith("Time"): 
                        print("TMAG5170 Recording started...")
                        is_recording = True
                    
                    # Skip parsing this line (it's either the heartbeat or the header)
                    continue 
                
                if line == "DONE":
                    print(f"\nRecording complete. Saved to {OUTPUT_FILE}")
                    break

                try:
                    # Parse the raw stream: [time, x, y, z, sin, cos]
                    parts = line.split(',')
                    if len(parts) == 6:
                        t_ms = parts[0]
                        x_raw = int(parts[1])
                        y_raw = int(parts[2])
                        z_raw = int(parts[3])

                        # --- 5170 PHYSICS MATH ---
                        x_mT = (x_raw / 32768.0) * 25.0
                        y_mT = (y_raw / 32768.0) * 25.0
                        z_mT = (z_raw / 32768.0) * 25.0

                        xy_deg = math.degrees(math.atan2(-y_mT, -x_mT)) % 360
                        xz_deg = math.degrees(math.atan2(-z_mT, -2.0 * x_mT)) % 360
                        
                        file.write(f"{t_ms},{x_mT:.4f},{y_mT:.4f},{z_mT:.4f},{xy_deg:.2f},{xz_deg:.2f}\n")
                        
                except Exception:
                    continue 

except serial.SerialException as e:
    print(f"Error: Could not open serial port {SERIAL_PORT}.")
    print(e)
except KeyboardInterrupt:
    print("\nRecording stopped manually.")
finally:
    if 'ser' in locals() and ser.is_open:
        ser.close()