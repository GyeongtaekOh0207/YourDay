<?php
$server = "localhost";
$username = "root";
$password = "569856";
$dbname = "sensor_data";

$conn = new mysqli($servername, $username, $password, $dbname);

if($conn->connect_error){
	die("fail connect: " . $conn->connect_error);
}

$temperature = $_GET['temperature'];
$humidity = $_GET['humidity'];

$sql = "INSERT INTO dht_data (temperature, humidity) VALUES ('$temperature', '$humidity')";

if($conn->query($sql) === TRUE){
	echo "data success";
}else {
	echo "error: " . $conn->error;
}

$conn->close();
?>
