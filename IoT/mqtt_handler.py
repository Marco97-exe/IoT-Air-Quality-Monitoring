import random

from paho.mqtt import client as mqtt_client
from to_influx import send_values_esp
import json
import csv
from datetime import datetime
from to_influx import get_data
import os




class Mqtt_handler:
    def __init__(self, measurement, tags) -> None:
        self.measurement = measurement
        self.tags = tags
        self.broker = 'broker.emqx.io'
        self.port = 1883
        self.topic = "esp32/IoT/sensor_values"
        # generate client ID with pub prefix randomly
        self.client_id = f'python-mqtt-{random.randint(0, 1000)}'
        # username = ''
        # password = ''
        self.client = mqtt_client.Client(self.client_id)
        self.client.connect(self.broker, self.port)
        self.client.on_connect = self.on_connect
        self.time_csv =  os.path.join(os.getcwd(), "dataset/times.csv")

    def on_connect(self,client, userdata, flags, rc):
        print("CONNECTED")
        print("Connected with result code: ", str(rc))
        client.subscribe(self.topic)
        print("subscribing to topic : "+self.topic)

    def on_message(self,client, userdata, msg):
        received = get_data()
        received = received.strftime("%H:%M:%S")
        content = json.loads(msg.payload.decode())
        second = content['second']
        minute = content['minute']
        hour = content['hour']
        sent = '{hour}:{minute}:{second}'.format(hour = hour, minute = minute, second= second)
        sent = format(datetime.strptime(sent, "%H:%M:%S"),"%H:%M:%S")

        with open(self.time_csv, 'a', newline='') as f:
            writer = csv.writer(f)
            data = [received, sent, 'MQTT']
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
            
        send_values_esp(self.measurement,self.tags, coordinates)
    def subscribe(self):
        self.client.on_message = self.on_message
        self.client.loop_forever()

if __name__ == "__main__":
    measurement = "Weather"
    tags = [("GPS", "44.49-11.313"), ("ID", "1")]
    mqtt_handler = Mqtt_handler(measurement=measurement, tags = tags)
    mqtt_handler.subscribe()
