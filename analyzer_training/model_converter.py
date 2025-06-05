import tensorflow as tf
import joblib

# Load the trained model
rf_model = joblib.load('trained_model.joblib')

class RandomForestWrapper(tf.Module):
    def __init__(self, model):
        super(RandomForestWrapper, self).__init__()
        self.model = model

    @tf.function(input_signature=[tf.TensorSpec(shape=[None, 60004], dtype=tf.float32)])
    def __call__(self, inputs):
        inputs_np = tf.make_ndarray(inputs)  # Convert tensor to NumPy array
        predictions = self.model.predict_proba(inputs_np)[:, 1]
        return tf.convert_to_tensor(predictions, dtype=tf.float32)

# Convert to TensorFlow Lite model
def convert_to_tflite(model, input_shape):
    # Create a concrete function
    @tf.function(input_signature=[tf.TensorSpec(shape=input_shape, dtype=tf.float32)])
    def serve_fn(inputs):
        return model(inputs)

    concrete_func = serve_fn.get_concrete_function()

    # Convert to TensorFlow Lite model
    converter = tf.lite.TFLiteConverter.from_concrete_functions([concrete_func])
    tflite_model = converter.convert()

    return tflite_model

# Define input shape based on training data
input_shape = [None, 60004]

# Wrap the trained RandomForest model
rf_wrapper = RandomForestWrapper(rf_model)

# Convert the model to TFLite
tflite_model = convert_to_tflite(rf_wrapper, input_shape)

# Save the TFLite model
with open('model.tflite', 'wb') as f:
    f.write(tflite_model)

print("TFLite model conversion complete.")