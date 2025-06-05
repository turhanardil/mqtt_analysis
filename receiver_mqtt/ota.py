import paho.mqtt.client as mqtt
import os

# Replace with your MQTT broker address and port
MQTT_BROKER    = "<YOUR_MQTT_BROKER_ADDRESS>"
MQTT_PORT      = 1884
# Replace with your OTA topic
MQTT_TOPIC_OTA = "<YOUR_MQTT_TOPIC_OTA>"

# Replace with the path to your firmware binary
FIRMWARE_PATH  = "<PATH_TO_YOUR_FIRMWARE_FILE>"
CHUNK_SIZE     = 1024  # 1 KB chunks

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT Broker!")
    else:
        print(f"Failed to connect, return code {rc}\n")

def on_publish(client, userdata, mid):
    print(f"Message published: {mid}")

# Create an MQTT client instance (using MQTT v3.1.1)
client = mqtt.Client(protocol=mqtt.MQTTv311)
client.on_connect = on_connect
client.on_publish = on_publish

# Connect to the broker
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_start()

# Verify firmware file exists
if not os.path.isfile(FIRMWARE_PATH):
    print("Firmware file not found at:", FIRMWARE_PATH)
    exit(1)

# Read and publish the firmware in chunks
with open(FIRMWARE_PATH, "rb") as firmware:
    chunk = firmware.read(CHUNK_SIZE)
    while chunk:
        result = client.publish(MQTT_TOPIC_OTA, chunk)
        status = result.rc
        if status == mqtt.MQTT_ERR_SUCCESS:
            print(f"Sent chunk of size {len(chunk)}")
        else:
            print(f"Failed to send chunk to topic {MQTT_TOPIC_OTA}")
        chunk = firmware.read(CHUNK_SIZE)

# Clean up
client.loop_stop()
client.disconnect()
