import numpy as np
import pandas as pd
import json
from scipy.fft import fft
from scipy.signal import butter, filtfilt

# Define parameters
num_samples = 1000  # Total samples
sampling_rate = 1000  # 1 kHz
time_series_length = 60 * sampling_rate  # 60 seconds of data at 1kHz
anomaly_start_indices = np.random.choice(range(num_samples), size=200, replace=False)  # Randomly select 200 anomaly start times
time = pd.date_range(start='2024-01-01', periods=num_samples, freq='T')  # One-minute intervals

print("Generating synthetic time series data...")

# Function to calculate THD for a voltage signal
def calculate_thd(voltage):
    fft_vals = np.abs(fft(voltage))
    fundamental = fft_vals[1]
    harmonics = fft_vals[2:]
    thd = np.sqrt(np.sum(harmonics**2)) / fundamental
    return thd

# Function to generate realistic voltage signal with anomalies
def generate_voltage_signal(length, freq, voltage_range, index, is_anomalous):
    t = np.linspace(0, 1, length)
    if is_anomalous:
        # Voltage anomaly simulation (spikes and dips)
        voltage = np.sin(2 * np.pi * freq * t) * np.random.uniform(*voltage_range)
        spike_or_dip = np.random.choice([1.3, 0.7, 1.0], size=len(t), p=[0.1, 0.1, 0.8])  # Increased chance of spike or dip
        voltage *= spike_or_dip
        # Additional noise for harmonics
        harmonics = np.sum([np.sin(2 * np.pi * (freq * (i + 1)) * t) * 0.05 for i in range(5)], axis=0)
        voltage += harmonics + np.random.normal(0, 0.1 * (voltage_range[1] - voltage_range[0]), length)
    else:
        # Normal operation
        voltage = np.sin(2 * np.pi * freq * t) * np.random.uniform(*voltage_range)
        voltage += np.random.normal(0, 0.05 * (voltage_range[1] - voltage_range[0]), length)
    return voltage.tolist()

# Function to generate other features with anomalies
def generate_other_features(index, length, is_anomalous):
    if is_anomalous:
        current = np.random.uniform(0.05, 5.5) * np.random.uniform(0.7, 1.3)  # Increased range for anomalies
        active_power = current * np.random.uniform(220, 240) * np.random.uniform(0.7, 1.3)  # Voltage effect
        thd_voltage = np.random.uniform(5, 20)  # Higher THD due to non-linear loads
        frequency = np.random.uniform(44, 66)  # Frequency fluctuations
    else:
        current = np.random.uniform(0.05, 5.5)
        active_power = current * np.random.uniform(220, 240)
        thd_voltage = np.random.uniform(0, 10)
        frequency = np.random.uniform(45, 65)
    return current, active_power, thd_voltage, frequency

# Generating synthetic time series data
def generate_time_series_data(num_samples, length, anomaly_start_indices, freq=50, voltage_range=(10, 300)):
    data = []
    for i in range(num_samples):
        is_anomalous = i in anomaly_start_indices
        voltage = generate_voltage_signal(length, freq, voltage_range, i, is_anomalous)
        current, active_power, thd_voltage, frequency = generate_other_features(i, length, is_anomalous)
        thd_voltage = calculate_thd(voltage)  # Calculate THD for voltage
        data.append((voltage, current, active_power, thd_voltage, frequency, int(is_anomalous)))
    return data

# Generate features with anomalies
data = generate_time_series_data(num_samples, time_series_length, anomaly_start_indices, voltage_range=(10, 300))
voltage_l1, current_l1, active_power_l1, thd_voltage_l1, frequency, target = zip(*data)
print("Data generated.")

# Combine into a DataFrame
synthetic_data = pd.DataFrame({
    'Time': time,
    'Voltage_L1': voltage_l1,
    'Current_L1': current_l1,
    'Active_Power_L1': active_power_l1,
    'THD_Voltage_L1': thd_voltage_l1,
    'Frequency': frequency,
    'target': target  # Add the target column
})

# Convert list to JSON string for voltage signal
synthetic_data['Voltage_L1'] = synthetic_data['Voltage_L1'].apply(json.dumps)
print("DataFrame created and lists converted to JSON strings.")

# Save to CSV
synthetic_data.to_csv('synthetic_mpr53s_data_with_realistic_failures.csv', index=False)
print("Data saved to CSV.")
