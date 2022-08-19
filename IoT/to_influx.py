import influxdb_client, os, time
from influxdb_client import Point
from influxdb_client.client.write_api import SYNCHRONOUS
from datetime import datetime, timezone

token = os.environ.get("INFLUXDB_TOKEN")
org = "****"
url = "****"

client = influxdb_client.InfluxDBClient(url=url, token=token, org=org)

bucket="test"

write_api = client.write_api(write_options=SYNCHRONOUS)

#Method that sends to influx the data 
def send_values_esp(measurement: str, tags: list[tuple], coordinates):
  #Creating the point
  points = []
  for n in coordinates:
    point = Point(measurement)  
    for t in tags:
      point.tag(t[0], t[1])
    point.field(n[0], n[1])
    point.time(get_data())
    points.append(point)
    
  #sending point to influxdb
  write_api.write(bucket=bucket, org="Marco", record=points)
 
def send_predicted_values(measurement: str, tags: list[tuple], key, values, times):
  points = []

  for i in range(len(values)):
    point = Point(measurement)
    for t in tags:
      point.tag(t[0], t[1])
    point.field(key, values[i])
    point.time(times[i])
    points.append(point)
  write_api.write(bucket=bucket, org='Marco', record=points)

def get_data():
  utc_date = datetime.now(timezone.utc)
  return utc_date.astimezone()

def create_csv_from_influx():
  query_api = client.query_api()

  query = """from(bucket: "test")
    |> range(start: -1d)
    |> filter(fn: (r) => r["_measurement"] == "Weather")
    |> filter(fn: (r) => r["GPS"] == "44.49-11.313")
    |> filter(fn: (r) => r["ID"] == "1")
    |> filter(fn: (r) => r["_field"] == "AQI" or r["_field"] == "gas" or r["_field"] == "humidity" or r["_field"] == "temperature" or r["_field"] == "wifi")
    |> pivot(rowKey:["_time"], columnKey: ["_field"], valueColumn: "_value" )
    |> keep(columns: ["_time", "AQI", "gas", "temperature", "humidity", "wifi"])
    """
  tables = query_api.query_data_frame(query, org="Marco")

  # tables.to_csv('test_18_07.csv', columns=["_time", "AQI", "gas", "temperature", "humidity", "wifi"])






