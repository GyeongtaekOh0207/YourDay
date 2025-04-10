<?php
header('Content-Type: application/json');

$conn = mysqli_connect("localhost", "iot", "pwiot", "sensor_data");
if (!$conn) {
    die(json_encode(["error" => "Database connection failed"]));
}

$query = "SELECT temperature, humidity, illuminance, timestamp FROM dht_data 
          WHERE timestamp BETWEEN NOW() - INTERVAL 10 MINUTE AND NOW() 
          ORDER BY timestamp DESC";

$result = mysqli_query($conn, $query);

$data = array();
while ($row = mysqli_fetch_assoc($result)) {
    $data[] = [
        "time" => strtotime($row['timestamp']) * 1000,
        "temperature" => floatval($row['temperature']),
        "humidity" => floatval($row['humidity']),
        "illuminance" => floatval($row['illuminance'])
    ];
}

mysqli_close($conn);
echo json_encode($data);
?>
