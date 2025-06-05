import numpy as np
import pandas as pd
import json
from scipy.signal import butter, filtfilt
from datetime import datetime

# Load the data from CSV file with all data included
data = pd.read_csv('mqtt_all_messages_<timestamp>.csv')  # Replace <timestamp> with the actual timestamp of your CSV file

# Extract data for start register 1006 (current)
current_data = data[data['Start register'] == '1006']

# Extract data for start register 1007 (active power)
active_power_data = data[data['Start register'] == '1007']

# Extract data for start register 1009 (voltage)
voltage_data = data[data['Start register'] == '1009']

# Extract data for start register 1010 (frequency)
frequency_data = data[data['Start register'] == '1010']

# Convert Raw data to numeric
current_data['Raw data'] = pd.to_numeric(current_data['Raw data'])
active_power_data['Raw data'] = pd.to_numeric(active_power_data['Raw data'])
voltage_data['Raw data'] = pd.to_numeric(voltage_data['Raw data'])
frequency_data['Raw data'] = pd.to_numeric(frequency_data['Raw data'])

# Define low-pass and high-pass filter functions
def low_pass_filter(data, cutoff=65, fs=1000, order=5):
    nyquist = 0.5 * fs
    normal_cutoff = cutoff / nyquist
    b, a = butter(order, normal_cutoff, btype='low', analog=False)
    y = filtfilt(b, a, data)
    return y

def high_pass_filter(data, cutoff=45, fs=1000, order=5):
    nyquist = 0.5 * fs
    normal_cutoff = cutoff / nyquist
    b, a = butter(order, normal_cutoff, btype='high', analog=False)
    y = filtfilt(b, a, data)
    return y

# Apply low-pass and high-pass filters to the current signal
filtered_current = high_pass_filter(current_data['Raw data'].values, cutoff=45)
filtered_current = low_pass_filter(filtered_current, cutoff=65)

# Set current thresholds based on specifications
overcurrent_threshold = 5.5  # Upper limit based on specification
undercurrent_threshold = 0.05  # Lower limit based on specification

# Generate binary output based on the current thresholds
current_data['Current_Issue'] = np.where((filtered_current > overcurrent_threshold) | (filtered_current < undercurrent_threshold), 1, 0)

# Set voltage thresholds based on specifications for Line-to-Neutral
overvoltage_threshold = 300  # Upper limit based on specification
undervoltage_threshold = 10  # Lower limit based on specification

# Calculate THD (Total Harmonic Distortion) for voltage
fundamental = voltage_data['Raw data'].iloc[0] if not voltage_data.empty else 0
harmonics = voltage_data['Raw data'].iloc[1:].values if len(voltage_data) > 1 else []
thd = (sum(h**2 for h in harmonics) ** 0.5) / fundamental if fundamental else 0

# Generate binary output based on the voltage thresholds
voltage_data['Voltage_Issue'] = np.where((voltage_data['Raw data'] > overvoltage_threshold) | (voltage_data['Raw data'] < undervoltage_threshold), 1, 0)

# Combine the data back together
combined_data = pd.merge(current_data[['Raw data', 'Current_Issue']], voltage_data[['Raw data', 'Voltage_Issue']], left_index=True, right_index=True, suffixes=('_Current', '_Voltage'))
combined_data['Active_Power'] = active_power_data['Raw data'].values
combined_data['Frequency'] = frequency_data['Raw data'].values

# Save the data with binary output
combined_data.to_csv('processed_mqtt_data.csv', index=False)
print("Processed data saved to CSV.")
