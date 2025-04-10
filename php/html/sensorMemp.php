<!DOCTYPE html>
<html lang="ko">
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
            font-size: 24px;
            text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.2);
        }

        .alert-box {
            width: 60%;
            margin: 20px auto;
            padding: 15px;
            border-radius: 10px;
            background: rgba(255, 255, 255, 0.9);
            box-shadow: 0 4px 10px rgba(0, 0, 0, 0.3);
            font-size: 20px;
            color: #333;
        }

        .alert-message {
            font-weight: bold;
            font-size: 28px;
            color: #007BFF;
        }

        .timestamp {
            font-size: 20px;
            color: #555;
        }
    </style>
</head>
<body>
    <h1>실시간 온도 메시지</h1>

    <?php
        $conn = mysqli_connect("localhost", "iot", "pwiot", "sensor_data");
        if (!$conn) {
            die("Connection failed: " . mysqli_connect_error());
        }

        $query = "SELECT message, created_at FROM temp_alerts ORDER BY created_at DESC LIMIT 1";
        $result = mysqli_query($conn, $query);
        
        if ($row = mysqli_fetch_assoc($result)) {
            echo "<div class='alert-box'>";
            echo "<p class='alert-message'>" . htmlspecialchars($row['message']) . "</p>";
            echo "<p class='timestamp'>" . htmlspecialchars($row['created_at']) . "</p>";
            echo "</div>";
        } else {
            echo "<div class='alert-box'><p class='alert-message'>⏳ 데이터 없음</p></div>";
        }

        mysqli_close($conn);
    ?>
</body>
</html>
