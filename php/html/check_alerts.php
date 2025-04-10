<?php
$servername = "localhost";
$username = "root";
$password = "569856";
$dbname = "sensor_data";

// 데이터베이스 연결
$conn = new mysqli($servername, $username, $password, $dbname);
if ($conn->connect_error) {
    die("연결 실패: " . $conn->connect_error);
}

// 새로운 알림이 있는지 확인
$sql = "SELECT * FROM alerts ORDER BY created_at DESC LIMIT 5";
$result = $conn->query($sql);

if ($result->num_rows > 0) {
    while ($row = $result->fetch_assoc()) {
        echo "알림: " . $row["message"] . "\n";
    }
} else {
    echo "새로운 알림 없음\n";
}

$conn->close();
?>

