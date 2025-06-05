import numpy as np
import pandas as pd
import tensorflow as tf
import tensorflow_decision_forests as tfdf
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score, precision_score, recall_score, f1_score, roc_auc_score, confusion_matrix
import json

# Load data
data = pd.read_csv('synthetic_mpr53s_data_with_realistic_failures.csv', converters={'Voltage_L1': json.loads})
print("Data loaded from CSV.")

# Split the data into features and target
X = data[['Voltage_L1', 'Current_L1', 'Active_Power_L1', 'THD_Voltage_L1', 'Frequency']]
y = data['target']  # Ensure your target column is named 'target'

# Flatten the nested JSON structure for Voltage_L1
voltage_l1_expanded = pd.DataFrame(X['Voltage_L1'].tolist())
X = X.drop(columns=['Voltage_L1']).join(voltage_l1_expanded)

# Ensure all column names are strings
X.columns = X.columns.astype(str)

# Split the data into training and testing sets
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

# Convert the DataFrame to TensorFlow Dataset and include batching
batch_size = 32
train_dataset = tf.data.Dataset.from_tensor_slices((dict(X_train), y_train)).batch(batch_size)
test_dataset = tf.data.Dataset.from_tensor_slices((dict(X_test), y_test)).batch(batch_size)

# Train the TensorFlow Decision Forests model
model = tfdf.keras.RandomForestModel(task=tfdf.keras.Task.CLASSIFICATION)
model.fit(train_dataset)

# Evaluate the model
y_pred = model.predict(test_dataset)
y_pred_labels = np.argmax(y_pred, axis=1)

accuracy = accuracy_score(y_test, y_pred_labels)
precision = precision_score(y_test, y_pred_labels, average='binary')
recall = recall_score(y_test, y_pred_labels, average='binary')
f1 = f1_score(y_test, y_pred_labels, average='binary')
roc_auc = roc_auc_score(y_test, y_pred_labels)
conf_matrix = confusion_matrix(y_test, y_pred_labels)

print(f"Accuracy: {accuracy}")
print(f"Precision: {precision}")
print(f"Recall: {recall}")
print(f"F1 Score: {f1}")
print(f"AUC-ROC: {roc_auc}")
print("Confusion Matrix:")
print(conf_matrix)

# Save the model
model.save("trained_model")
