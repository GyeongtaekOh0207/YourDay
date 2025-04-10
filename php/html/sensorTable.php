<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta http-equiv="refresh" content="30">
    <style>
        body {
            font-family: Arial, sans-serif;
            text-align: center;
            background: linear-gradient(to right, #74ebd5, #acb6e5);
            margin: 0;
            padding: 20px;
            color: #333;
        }
        h1 {
            color: white;
            font-size: 28px;
            text-shadow: 3px 3px 6px rgba(0, 0, 0, 0.5);
        }
        p {
            font-size: 20px;
            color: #fff;
        }
        table {
            width: 90%;
            margin: 20px auto;
            border-collapse: collapse;
            background: rgba(255, 255, 255, 0.9);
            box-shadow: 0 4px 10px rgba(0, 0, 0, 0.3);
            border-radius: 10px;
            overflow: hidden;
        }
        th, td {
            padding: 14px;
            border: 1px solid #ddd;
            text-align: center;
            font-size: 18px;
        }
        th {
            background-color: #007BFF;
            color: white;
        }
        td {
            background: rgba(255, 255, 255, 0.8);
        }
    </style>
</head>
<body>
    <h1>너의 하루는? 🌞❓</h1>
    <p><b>실시간 조도, 온도 및 습도 데이터</b></p>

    <table>
        <tr>
            <th>조도 (맑음 정도)</th>
            <th>온도 (°C)</th>
            <th>습도 (%)</th>
            <th>측정 시간</th>
        </tr>

        <?php
            $conn = mysqli_connect("localhost", "iot", "pwiot", "sensor_data");
            if (!$conn) {
                die("Connection failed: " . mysqli_connect_error());
            }
            
            $query = "SELECT * FROM dht_data WHERE timestamp BETWEEN NOW() - INTERVAL 80 SECOND AND NOW() ORDER BY timestamp DESC";
            $result = mysqli_query($conn, $query);
            
            while ($row = mysqli_fetch_assoc($result)) {
                echo "<tr>";
                echo "<td>" . htmlspecialchars($row['Illuminance']) . " 맑음 정도</td>";
                echo "<td>" . htmlspecialchars($row['temperature']) . "°C</td>";
                echo "<td>" . htmlspecialchars($row['humidity']) . "%</td>";
                echo "<td>" . htmlspecialchars($row['timestamp']) . "</td>";
                echo "</tr>";
            }
            
            mysqli_close($conn);
        ?>
    </table>
</body>
</html>
