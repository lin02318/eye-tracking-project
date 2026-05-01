# Wireless, Magnet-Based Eye Tracking in Lab Animals

Authors: Thomas Lin (tal96), Jialin Song (js3885)

Advisor: Professor Hunter Adams (vha3)

## Introduction

### Motivation
Current video-based eye-tracking methods for freely moving lab animals are bulky, computationally heavy, and uncomfortable.

### The Alternative
This project investigates a lightweight alternative: tracking eye movements by implanting a small magnet on the eye and measuring changes in the magnetic field.

### Project Goal
To validate the feasibility, accuracy, and noise characteristics of magnetic eye-tracking by comparing two different sensor architectures.

## Materials and Methods
To evaluate feasibility, we built a physical eye model and a high-speed data acquisition system.

### Sensor Selection
- TMAG5170: Digital 3-axis linear Hall-effect sensor (SPI interface). Measures raw $B_x$, $B_y$, and $B_z$ magnetic fields.
- TMAG6180: Analog Anisotropic Magneto Resistive (AMR) angle sensor. Directly outputs sine and cosine voltages ($V_{sin}$, $V_{cos}$) representing the Z-axis angle.

### Hardware Architecture
- A Raspberry Pi Pico (RP2040) acts as the central microcontroller, utilizing C and Protothreads for precise hardware-timed sampling.
- Data is streamed via USB serial to a PC for automated Python-based physics calculations and frequency analysis.

### Experimental Setup
- A ping-pong ball with an attached surface magnet simulates the animal eye
- A custom bracket with precise degree markings holds the ball, establishing our ground-truth baseline for measurements.

[ Picture: System Schematic Diagram ]  
[ Picture: Photos of the RP2040, Sensors, and Ping-Pong ball setup ]

## Results

### Angle Accuracy Assessment
We recorded measured angles against the ground-truth bracket in 22.5° increments.

#### Calculation Methods
- TMAG5170:$$\text{Angle} = \text{atan2}(-B_y, -B_x) \times \frac{180}{\pi}$$
- TMAG6180:$$\text{Angle} = \frac{\text{atan2}(V_{sin}, V_{cos})}{2} \times \frac{180}{\pi}$$(Note: AMR sensors have a 180° measurement range, causing wrap-around).

#### Initial Findings
Both sensors exhibited similar baseline accuracy (error generally < 10°).

[ Picture: Angle Accuracy / Error Chart ]

#### The Challenge
Raw data contained significant noise. Stable readings initially required a heavy digital low-pass filter (sampling at 10 Hz, averaging 50 samples over 5 seconds). Because lab animals exhibit rapid eye movements, heavy low-pass filtering is impractical.


### High-Frequency Noise Characterization

To determine which sensor is best for live-animal applications, we removed the low-pass filter, increased the hardware sampling rate to a deterministic 100 Hz, and captured a 5-second burst (500 samples) at a fixed 45° angle.

- TMAG5170 (Digital 3-Axis Linear Snensor): Displayed high inherent noise. The signal exhibited up to 10° of peak-to-peak jitter, with an RMS Noise (Standard Deviation) of ~2.03°.
- TMAG6180 (Analog AMR Angle Sensor): Displayed excellent stability. The peak-to-peak jitter was only ~1°, with an RMS Noise of just ~0.27°.
- Spectrum Analysis: Fast Fourier Transform (FFT) calculations showed a relatively uniform noise floor across the 0–50 Hz spectrum for both devices.

[ Picture: Angle-Over-Time and Frequency Spectrum Plot for both sensors ]

## Conclusion

Both magnetic sensors successfully track magnet orientation within an acceptable margin of accuracy.The TMAG5170 (digital 3-axis) suffers from a poor signal-to-noise ratio, making it unsuitable for tracking rapid eye movements without aggressive filtering that would destroy temporal data.The TMAG6180 (analog AMR) provides a significantly cleaner signal (~8x less RMS noise) and is the vastly superior choice for future miniaturized, wireless tracking in live animal subjects.

## Acknowledgements

We would like to express our deepest gratitude to our project advisor, Professor Hunter Adams, for his continuous guidance and technical mentorship. We also extend our sincere thanks to Professor Madineh Sedigh-Sarvestani from the Cornell Department of Neurobiology and Behavior for her invaluable insights and expertise. Finally, we thank the Cornell ECE department for providing the laboratory resources that made this prototype possible.
