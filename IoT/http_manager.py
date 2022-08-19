
from flask import Flask, render_template, request, jsonify
import os
from to_influx import send_values_esp, get_data
import csv
from mqtt_handler import Mqtt_handler
from datetime import datetime, timezone


app = Flask(__name__)

conf = {
    "SAMPLE_FREQUENCY": 10000, #ms (10s)
    "MIN_GAS_VALUE": 100,
    "MAX_GAS_VALUE": 1000,
    "TECHNOLOGY": 0,
    }
"""

HTTP HANDLER

"""




time_csv = os.path.join(os.getcwd(), "dataset/times.csv")
measurement = "Weather"
tags = [("GPS", "44.49-11.313"), ("ID", "1")]

@app.route('/get_config', methods=['GET'])
def get_configuration():
    return conf

@app.route('/values', methods=['POST'])
def parse_request():
    received = get_data().time()
    received = received.strftime("%H:%M:%S")
    content = request.json


    second = content['second']
    minute = content['minute']
    hour = content['hour']
    sent = '{hour}:{minute}:{second}'.format(hour = hour, minute = minute, second= second)

    sent = format(datetime.strptime(sent, "%H:%M:%S"),"%H:%M:%S")
    with open(time_csv, 'a', newline='') as f:
        writer = csv.writer(f)
        data = [received, sent, 'HTTP']
        writer.writerow(data)

    del content["GPS_Latitude"]
    del content["GPS_Longitude"]
    del content["ID"]
    del content['second']
    del content['minute']
    del content['hour']

    keys = content.keys()
    values = content.values()

    coordinates = zip(keys,values)
    send_values_esp(measurement, tags, coordinates)
    return jsonify(content)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/configuration', methods=['POST', 'GET'])
def setup_configuration():
    if request.method == 'POST':
        conf["SAMPLE_FREQUENCY"] =  request.form["sample_frequency"]
        conf["MAX_GAS_VALUE"] = request.form["max_gas_value"]
        conf["MIN_GAS_VALUE"] = request.form["min_gas_value"]
        conf["TECHNOLOGY"] = request.form["technology"]
        return render_template('configuration.html')
    else:
        return render_template('configuration.html')


if __name__ == "__main__":
    app.run(host='0.0.0.0', port=1025,debug=True)

    
