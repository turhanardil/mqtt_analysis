import paho.mqtt.client as mqtt
import time
import re
import pandas as pd
from collections import Counter
from datetime import datetime

# MQTT settings (replace these placeholders with your own values)
broker = "<YOUR_MQTT_BROKER_ADDRESS>"
port = 1884               # Replace with your MQTT broker port if different
topic = "<YOUR_MQTT_TOPIC>"

qos = 0

# Lists to store incoming messages and register counts
messages_1006 = []
all_messages = []
start_register_counts = Counter()

# Function to preprocess the message to extract key-value pairs
def preprocess_message(raw_message):
    # Remove all line breaks and unnecessary spaces
    fixed_message = raw_message.replace('\n', '').replace('\r', '').strip()

    # Replace double colons with single colons
    fixed_message = fixed_message.replace('::', ':')

    # Remove any extra commas at the beginning or end of the message
    fixed_message = fixed_message.strip(', ')

    # Replace multiple spaces with single spaces
    fixed_message = re.sub(r'\s+', ' ', fixed_message)

    # Extract the relevant fields using more precise regex patterns
    start_register = re.search(r'"Start register":\s*(\d+)', fixed_message)
    raw_data = re.search(r'"Raw data":\s*(\d+)', fixed_message)

    # Filter only the relevant fields
    filtered_message = {
        "Start register": start_register.group(1) if start_register else "",
        "Raw data": raw_data.group(1) if raw_data else ""
    }

    # Count the occurrence of each start register
    if start_register:
        start_register_counts[start_register.group(1)] += 1

    # Collect data for specific start registers
    if start_register and start_register.group(1) == '1006':
        messages_1006.append(filtered_message)

    # Collect all messages
    all_messages.append(filtered_message)

    return filtered_message

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, reasonCode, properties):
    if reasonCode == 0:
        print("Connected successfully with result code", reasonCode)
        client.subscribe(topic, qos)
    else:
        print("Connection failed with result code", reasonCode)

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    try:
        # Attempt to decode the message with 'utf-8' encoding
        message = msg.payload.decode('utf-8', 'ignore')
        print(f"Topic: {msg.topic}, Message: {message}")

        # Preprocess the message to extract key-value pairs
        processed_message = preprocess_message(message)

        print("Processed message added to list:", processed_message)
    except Exception as e:
        print(f"Error processing message: {e}")

# Create an MQTT client instance with version 2 callback API
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)

# Assign the on_connect and on_message callbacks
client.on_connect = on_connect
client.on_message = on_message

# Connect to the broker
client.connect(broker, port, 60)

# Start the loop
client.loop_start()

# Collect data for 1 minute (adjust time as needed)
time.sleep(60)

# Stop the loop
client.loop_stop()

# Disconnect from the broker
client.disconnect()

# Now 'messages' list contains all collected MQTT messages in a structured format
print(f"Collected {len(messages_1006)} messages with '1006' Start register.")
print(f"Collected {len(all_messages)} messages in total.")

# Save '1006' start register messages to a CSV file
timestamp = datetime.now().strftime("%Y%m%d%H%M%S")
csv_filename_1006 = f"mqtt_messages_1006_{timestamp}.csv"
df_1006 = pd.DataFrame(messages_1006)
df_1006.to_csv(csv_filename_1006, index=False)
print(f"'1006' Start register messages saved to {csv_filename_1006}.")

# Save start register counts to a CSV file
csv_filename_counts = f"mqtt_register_counts_{timestamp}.csv"
df_counts = pd.DataFrame(start_register_counts.items(), columns=["Start register", "Count"])
df_counts.to_csv(csv_filename_counts, index=False)
print(f"Start register counts saved to {csv_filename_counts}.")

# Save all messages to another CSV file
csv_filename_all = f"mqtt_all_messages_{timestamp}.csv"
df_all = pd.DataFrame(all_messages)
df_all.to_csv(csv_filename_all, index=False)
print(f"All messages saved to {csv_filename_all}.")

# Calculate THD (Total Harmonic Distortion) based on the received values (for voltage data in start register 1009)
voltage_messages = [msg for msg in all_messages if msg["Start register"] == '1009']
if voltage_messages:
    voltage_values = [int(msg["Raw data"]) for msg in voltage_messages]
    fundamental = voltage_values[0] if voltage_values else 0
    harmonics = voltage_values[1:] if len(voltage_values) > 1 else []
    thd = (sum(h**2 for h in harmonics) ** 0.5) / fundamental if fundamental else 0
    print(f"Calculated THD for voltage: {thd}")

print("Data processing completed.")
