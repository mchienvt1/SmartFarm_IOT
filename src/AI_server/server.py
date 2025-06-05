import eventlet
eventlet.monkey_patch()  # ⚠️ Gọi sớm nhất

import os
import json
import numpy as np
import joblib
import traceback
from flask import Flask, render_template, request
from flask_socketio import SocketIO, emit
from tensorflow.keras.models import load_model
from collections import deque

# Buffer để lưu dữ liệu cảm biến (window_size = 6)
sensor_buffer = deque(maxlen=6)

# --- Cấu hình đường dẫn (cập nhật theo tên file từ training code) ---
MODEL_PATH = './env_model.h5'
FEATURE_SCALER_PATH = './env_feature_scaler.pkl'
TARGET_SCALER_PATH = './env_target_scaler.pkl'

# --- Khởi tạo Flask App ---
app = Flask(__name__, template_folder='templates')
app.config['SECRET_KEY'] = 'secret_key'

# --- Cấu hình SocketIO ---
socketio = SocketIO(
    app,
    cors_allowed_origins="*",
    async_mode='eventlet',
    logger=True,
    engineio_logger=True,
    ping_timeout=60,
    ping_interval=25,
    allow_upgrades=True,
    websocket=True
)

# --- Load mô hình và scaler ---
print("🔄 Đang load mô hình và scaler...")
try:
    model = load_model(MODEL_PATH, compile=False)
    feature_scaler = joblib.load(FEATURE_SCALER_PATH)
    target_scaler = joblib.load(TARGET_SCALER_PATH)
    print("✅ Đã load thành công.")
    print(f"📊 Input shape mô hình: {model.input_shape}")
    print(f"📊 Output shape mô hình: {model.output_shape}")
except Exception as e:
    print("❌ Lỗi khi load mô hình hoặc scaler:", e)
    traceback.print_exc()
    exit()

# --- Route trả về giao diện chính ---
@app.route('/')
def index():
    return render_template('index.html')

# --- Sự kiện khi client kết nối ---
@socketio.on('connect')
def handle_connect():
    sid = request.sid
    print(f"📡 Client connected! SID: {sid}")
    emit('connect_response', {'status': 'connected'})

@socketio.on('disconnect')
def handle_disconnect():
    sid = request.sid
    print(f"🔌 Client disconnected! SID: {sid}")

# --- Hàm xác định status dựa trên giá trị ---
def determine_status_from_values(temp, humidity):
    """
    Xác định status dựa trên giá trị nhiệt độ và độ ẩm
    Bạn có thể tùy chỉnh logic này theo yêu cầu thực tế
    """
    if temp < 15 or temp > 35 or humidity < 30 or humidity > 80:
        return 'critical'
    elif temp < 20 or temp > 30 or humidity < 40 or humidity > 70:
        return 'warning'
    else:
        return 'normal'

# --- Hàm dự đoán giá trị tiếp theo ---
def predict_next_values(model, scaler, target_scaler, last_sequence):
    """
    Predict next temperature and humidity values
    
    Args:
        model: trained model
        scaler: fitted feature scaler
        target_scaler: fitted target scaler  
        last_sequence: array of shape (window_size, 2) with last 6 [temp, humidity] pairs
    
    Returns:
        predicted [temperature, humidity] in original scale
    """
    try:
        # Normalize the input
        last_sequence_scaled = scaler.transform(last_sequence)
        
        # Reshape for model input: (1, window_size, 2)
        last_sequence_scaled = last_sequence_scaled.reshape(1, 6, 2)
        
        # Make prediction
        pred_scaled = model.predict(last_sequence_scaled, verbose=0)
        
        # Convert back to original scale
        pred_original = target_scaler.inverse_transform(pred_scaled)
        
        return pred_original[0]
    except Exception as e:
        print(f"❌ Lỗi trong quá trình dự đoán: {e}")
        traceback.print_exc()
        return None

# --- Nhận dữ liệu cảm biến từ ESP32 ---
@socketio.on('sensor_data')
def handle_sensor_data(data):
    global sensor_buffer
    try:
        print("📥 Dữ liệu nhận được:", data)

        if isinstance(data, str):
            sensor_values = json.loads(data)
        else:
            sensor_values = data

        # Kiểm tra định dạng dữ liệu
        # Dự kiến: [temperature, humidity]
        if not isinstance(sensor_values, list) or len(sensor_values) < 2:
            emit('error', {'message': 'Dữ liệu phải là mảng chứa 2 giá trị [temperature, humidity]'})
            return

        # Lấy temperature và humidity
        env_temp = float(sensor_values[0])
        env_humi = float(sensor_values[1])
        
        # Tạo feature vector chỉ gồm 2 giá trị: [env_temp, env_humi]
        feature_vector = [env_temp, env_humi]
        
        # Thêm vào buffer
        sensor_buffer.append(feature_vector)
        
        # Xác định status hiện tại
        current_status = determine_status_from_values(env_temp, env_humi)

        # Tạo dữ liệu để gửi về dashboard
        dashboard_data = {
            'temperature': env_temp,
            'humidity': env_humi,
            'status': current_status,
            'buffer_size': len(sensor_buffer)
        }

        # Phát lại dữ liệu để hiển thị dashboard
        socketio.emit('sensor_data_received', dashboard_data)

        # Kiểm tra xem đã đủ dữ liệu để dự đoán chưa
        if len(sensor_buffer) < 6:
            print(f"🕐 Đã nhận {len(sensor_buffer)}/6 mẫu. Chưa đủ để dự đoán.")
            return

        # Chuẩn bị dữ liệu cho dự đoán
        recent_data = np.array(sensor_buffer)
        print(f"📊 Hình dạng dữ liệu buffer: {recent_data.shape}")

        # Kiểm tra shape đúng (6, 2)
        if recent_data.shape != (6, 2):
            print(f"❌ Shape không đúng. Mong đợi (6, 2), nhận được {recent_data.shape}")
            return

        # Dự đoán giá trị tiếp theo
        prediction = predict_next_values(model, feature_scaler, target_scaler, recent_data)
        
        if prediction is None:
            emit('error', {'message': 'Lỗi trong quá trình dự đoán'})
            return

        # Chuẩn bị response
        predicted_temp = round(float(prediction[0]), 2)
        predicted_humi = round(float(prediction[1]), 2)
        
        # Xác định status dự đoán
        predicted_status = determine_status_from_values(predicted_temp, predicted_humi)

        response = {
            'predicted_temperature': predicted_temp,
            'predicted_humidity': predicted_humi,
            'predicted_status': predicted_status,
            'current_temperature': env_temp,
            'current_humidity': env_humi,
            'current_status': current_status,
            'timestamp': len(sensor_buffer),
            'buffer_data': [{'temp': round(row[0], 2), 'humi': round(row[1], 2)} for row in sensor_buffer]
        }

        print("📤 Gửi dự đoán:", response)
        socketio.emit('prediction', response)

    except Exception as e:
        print("❌ Lỗi xử lý dữ liệu:", e)
        traceback.print_exc()
        emit('error', {'message': f'Lỗi xử lý: {str(e)}'})

# --- API endpoint để reset buffer ---
@socketio.on('reset_buffer')
def handle_reset_buffer():
    global sensor_buffer
    sensor_buffer.clear()
    print("🔄 Buffer đã được reset")
    emit('buffer_reset', {'status': 'success', 'message': 'Buffer đã được reset'})

# --- API endpoint để lấy thông tin mô hình ---
@socketio.on('get_model_info')
def handle_get_model_info():
    try:
        model_info = {
            'input_shape': model.input_shape,
            'output_shape': model.output_shape,
            'buffer_size': len(sensor_buffer),
            'max_buffer_size': sensor_buffer.maxlen,
            'features': ['temperature', 'humidity'],
            'targets': ['temperature', 'humidity'],
            'window_size': 6
        }
        emit('model_info', model_info)
    except Exception as e:
        emit('error', {'message': f'Lỗi lấy thông tin mô hình: {str(e)}'})

# --- Test endpoint để gửi dữ liệu giả ---
@socketio.on('test_data')
def handle_test_data():
    """
    Gửi dữ liệu test để kiểm tra hệ thống
    """
    import random
    
    # Tạo dữ liệu test
    test_temp = round(random.uniform(20, 30), 2)
    test_humi = round(random.uniform(50, 70), 2)
    test_data = [test_temp, test_humi]
    
    print(f"🧪 Gửi dữ liệu test: {test_data}")
    handle_sensor_data(test_data)

# --- Test endpoint để gửi nhiều dữ liệu test ---
@socketio.on('test_multiple_data')
def handle_test_multiple_data():
    """
    Gửi nhiều dữ liệu test để đủ buffer cho dự đoán
    """
    import random
    import time
    
    print("🧪 Gửi 6 mẫu dữ liệu test để kiểm tra dự đoán...")
    
    for i in range(6):
        test_temp = round(random.uniform(20, 30), 2)
        test_humi = round(random.uniform(50, 70), 2)
        test_data = [test_temp, test_humi]
        
        print(f"🧪 Mẫu {i+1}/6: {test_data}")
        handle_sensor_data(test_data)
        
        # Nhỏ delay để thấy rõ quá trình
        eventlet.sleep(0.1)

# --- API endpoint để lấy buffer hiện tại ---
@socketio.on('get_buffer')
def handle_get_buffer():
    try:
        buffer_data = {
            'buffer': [{'temp': round(row[0], 2), 'humi': round(row[1], 2)} for row in sensor_buffer],
            'size': len(sensor_buffer),
            'max_size': sensor_buffer.maxlen,
            'ready_for_prediction': len(sensor_buffer) >= 6
        }
        emit('buffer_data', buffer_data)
    except Exception as e:
        emit('error', {'message': f'Lỗi lấy buffer: {str(e)}'})

# --- Khởi động server ---
if __name__ == '__main__':
    print("=" * 60)
    print("--- 🚀 Environment Monitoring Server đang khởi động ---")
    print(f"🔧 Chế độ async: {socketio.async_mode}")
    print(f"📁 Model path: {MODEL_PATH}")
    print(f"📁 Feature scaler: {FEATURE_SCALER_PATH}")
    print(f"📁 Target scaler: {TARGET_SCALER_PATH}")
    print(f"📊 Buffer size: {sensor_buffer.maxlen}")
    print(f"📊 Input features: Temperature, Humidity")
    print(f"📊 Output targets: Temperature, Humidity")
    print("=" * 60)
    socketio.run(app, host='0.0.0.0', port=5000, debug=True, use_reloader=False, allow_unsafe_werkzeug=True)