import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from scipy.fft import fft, fftfreq

# 读取文件
df1 = pd.read_csv("tmag5170_data.csv")
df2 = pd.read_csv("tmag6180_data.csv")

data1 = df1["XY_Angle(deg)"].dropna().values
data2 = df2["AMR_Angle(deg)"].dropna().values

fs = 100  # 100 Hz

# 1 & 2: 平均值 & 标准差
for name, data in [("TMAG5170 XY_Angle", data1), ("TMAG6180 AMR_Angle", data2)]:
    print(f"{name}")
    print(f"  平均值: {np.mean(data):.4f} deg")
    print(f"  标准差: {np.std(data):.4f} deg\n")

# 3: 频谱图
def plot_spectrum(data, fs, title):
    N = len(data)
    yf = np.abs(fft(data - np.mean(data)))[:N//2]
    xf = fftfreq(N, 1/fs)[:N//2]
    plt.plot(xf, yf)
    plt.title(title)
    plt.xlabel("Frequency (Hz)")
    plt.ylabel("Magnitude")
    plt.grid(True)

plt.figure(figsize=(12, 5))
plt.subplot(1, 2, 1)
plot_spectrum(data1, fs, "TMAG5170 XY_Angle Spectrum")
plt.subplot(1, 2, 2)
plot_spectrum(data2, fs, "TMAG6180 AMR_Angle Spectrum")
plt.tight_layout()
plt.savefig("spectrum.png", dpi=150)
plt.show()