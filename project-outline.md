# Wireless, Magnet-Based Eye Tracking in Lab Animals

Thomas Lin (tal96), Jialin Song (js3885)

Advisor: Professor Hunter Adams (vha3)

## Introduction

( summarize the following paragraph in 1-2 sentences )
"
Motivation: Vision is actively controlled by eye movements, which help to localize objects of interest and to stabilize them on the retina. Therefore, eye movements must be tracked carefully in studies of visually guided behavior or visual neuroscience. Having good measurements of eye movements can provide a wealth of information about the inputs the brain receives, what the animal is attending to, the animal’s overall alertness level, and decision making. However, while it is straight forward to track eye movements in constrained animals and humans who can remain still, eye-tracking in small and freely moving lab animals is difficult. The state-of-the art solution employed by many labs requires a mirror and a camera in front of the animal’s eye (Figure 1). These trackers are not precise, nor comfortable for the animal, and are difficult to make wireless given the bandwidth of video. A promising alternative to this video-oculography approach is based on magnetic sensing. In preliminary proof-of-concept tests, a small magnet implanted on the surface of the eye produces large enough changes in the magnetic field that can be picked up by a magnetic sensing chip attached near the head. The goal of this project is to develop this idea further and build a wireless system for tracking 3D eye 
movements based on magnetic sensing. A successful device would have impact, and quick adoption, by many neuroscience laboratories worldwide."

( and followed by this )
The purpose of this project is to confirm whether it is feasible to track the eye movement of lab animals by implanting a small magnet on the surface of the eye, and measure the magnetic field to calculate the eye orientation. 

## Materials and Methods

We start by building a prototype to do some proof-of-concept tests. To measure the magnetic field, we chose two types of Hall-effect sensors to test compare their performance: TMAG5170 3-axis linear Hall-effect sensor and TMAG6180 analog AMR angle sensor.  
TMAG5170 3-axis linear Hall-effect sensor can measure the magnetic field along x, y, and z axis, and has an SPI interface.  
TMAG6180 analog AMR angle sensor can directly measure the angle of the magnetic field along z axis, and output the sine and cosine value of the angle with analog voltage.

We chose Raspberry Pi Pico to collect the data from the sensors and process the data. The processed data will then sent to PC serial monitor for us to further analyze the data.

( add a schematic of our PC/Raspberry Pi Pico/sensors system here, and also some pictures of the sensors and the ping-pong ball, bracket )

We  attach a small magnet on a ping-pong ball as the eye/magnet model, then we build a bracket with angle markings to hold the ping-pong ball.

We conduct the test by placing the ping-pong ball on the bracket, align the magnet direction with the angle markings on the bracket, and then read the sensor data.

## Results

### Angle Accuracy Test

We first conduct the angle accuracy test for both 3-axis linear sensor (TMAG5170) and analog AMR angle sensor (TMAG6180). We increase the magnet angle by 22.5 degrees at a time, and record the actual angle (the angle markings on the bracket) and the measured angle.  
For TMAG5170, the angle calculation is the following: `angle_deg = atan2(-By, -Bx) * (180/pi)`  
For TMAG6180, the angle calculation is the following: `angle_deg = (atan2(Vsin, Vcos)/2) * (180/pi)`

Since TMAG6180 only has the measurement range of 180 degrees, the angles from 180 to 360 degrees will wrap around ( become 0 to 180 degrees )  
The result shows that both sensors have similar accuracy, with error angle generally within 10 degrees.
( put the result chart here )

However, when we are conducting the accuracy test, we found that the noise is quite large, and we need to apply a digital low-pass filter to get a stable angle to record it. Details: we were running at sampling rate = 10 Hz, and the low-pass filter averages the most recent 50 samples (5 seconds) 

So we want to further investigate the noise on both sensors, since using a low-pass filter with such a low cutoff frequency might not be practical when it comes to lab animal experiments. 

### Noise Test

To measure the noise on both sensors, we first increase the sampling rate from 10 Hz to 100 Hz, so that we can measure the higher frequency noise. Next, we wrote a Python script to load the measured data for 5 seconds ( so it's 500 samples )  
We place the ping-pong ball at 45 degrees, and the following is the noise results:  
( put the angle-over-time and spectrum plot here )  
The noise on the 3-axis sensor (TMAG5170) is significantly larger, up to 10 degrees of difference between minimum measured angle and maximum measured angle, and the standard deviation of ~2 degrees.  
The noise on the analog angle sensor (TMAG6180) is quite small, only about 1 degree of difference between minimum measured angle and maximum measured angle, with the standard deviation of only ~0.27 degrees.  
From the spectrum results, the noise level is roughly the same across the frequency range of 0 to 50 Hz.

## Conclusion

Both the 3-axis sensor (TMAG5170) and the analog angle sensor (TMAG6180) can measure the magnet orientation accurately with a low-pass filter, and the error angle is roughly within 10 degrees. However, the noise on the 3-axis sensor is significantly larger, and for the real lab animal experiment, we might not want to low-pass the measured signal (because the eye movements could be fast, and if we low-pass the signal we will lose important information). Therefore, for future implementation on real lab animals, it might be better to use the analog angle sensor.

## Acknowledgements

???

## Further Information

???
