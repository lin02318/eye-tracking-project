import serial
import time
import math

# --- CONFIGURATION ---
SERIAL_PORT = 'COM3' 
BAUD_RATE = 115200
OUTPUT_FILE = 'tmag6180_data.csv'

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
        file.write("Time(ms),V_SIN(raw),V_COS(raw),AMR_Angle(deg)\n")
        
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
                        print("TMAG6180 Recording started...")
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
                        v_sin = int(parts[4])
                        v_cos = int(parts[5])

                        # --- 6180 PHYSICS MATH ---
                        amr_deg = (math.degrees(math.atan2(v_sin, v_cos)) / 2.0) % 180

                        file.write(f"{t_ms},{v_sin},{v_cos},{amr_deg:.2f}\n")
                        
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