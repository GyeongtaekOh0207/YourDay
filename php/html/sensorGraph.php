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
        
        /* 상단 중앙에 날짜 및 시간 표시 */
        .current-time {
            position: fixed;
            top: 30px; 
            left: 50%;
            transform: translateX(-50%);
            font-size: 32px;
            color: white;
            background: transparent;
            font-weight: bold;
            text-shadow: 3px 3px 6px rgba(0, 0, 0, 0.5);
            z-index: 1000;
            line-height: 1.5;
        }

        /* 메시지 간격 확보 */
        p {
            font-size: 20px;
            color: #fff;
            margin-top: 110px;
        }
        
        #chart_div {
            width: 90%; 
            max-width: 1200px;
            height: auto; 
            min-height: 400px;
            margin: 20px auto 30px; 
            background: rgba(255, 255, 255, 0.9);
            box-shadow: 0 4px 10px rgba(0, 0, 0, 0.3);
            border-radius: 10px;
            padding: 20px;
            display: flex;
            justify-content: center;
            align-items: center;
        }
    </style>
    <script src="https://www.gstatic.com/charts/loader.js"></script>
    <script>
        <?php
            $conn = mysqli_connect("localhost", "iot", "pwiot", "sensor_data");
            if (!$conn) {
                die("Connection failed: " . mysqli_connect_error());
            }

            // UTC+9로 변환된 타임스탬프 가져오기
            $query = "SELECT temperature, humidity, illuminance, 
                             timestamp
                      FROM dht_data 
                      WHERE timestamp BETWEEN NOW() - INTERVAL 80 SECOND AND NOW() 
                      ORDER BY timestamp DESC";

            $result = mysqli_query($conn, $query);

            $data = array(array('Time', 'Temperature (°C)', 'Humidity (%)', 'Illuminance (lux)')); 

            while ($row = mysqli_fetch_assoc($result)) {
                $timestamp = (strtotime($row['timestamp']) + 15 * 3600) * 1000;  // UTC+9 변환 후 밀리초 변환
                array_push($data, array($timestamp, floatval($row['temperature']), floatval($row['humidity']), floatval($row['illuminance'])));
            }
            
            mysqli_close($conn);
        ?>

        var data = <?= json_encode($data) ?>;
        var chart, dataTable;
        var options = {
            title: '온도, 습도, 조도 변화',
            titleTextStyle: {
                fontSize: 30,
                bold: true 
            },
            width: window.innerWidth * 0.8,
            height: window.innerHeight * 0.5,
            curveType: 'function',
            legend: { position: 'bottom' },
            backgroundColor: 'transparent'
        };

        google.charts.load('current', {packages:['corechart']});
        google.charts.setOnLoadCallback(drawChart);

        function drawChart() {
            dataTable = new google.visualization.DataTable();
            dataTable.addColumn('datetime', 'Time');
            dataTable.addColumn('number', 'Temperature (°C)');
            dataTable.addColumn('number', 'Humidity (%)');
            dataTable.addColumn('number', 'Illuminance (lux)');

            for (var i = 1; i < data.length; i++) {
            let correctTime = new Date(data[i][0]);
            console.log(correctTime);
            dataTable.addRow([correctTime, data[i][1], data[i][2], data[i][3]]);
        }

            chart = new google.visualization.LineChart(document.querySelector('#chart_div'));
            chart.draw(dataTable, options);
        }

        window.onresize = function () {
            var chartDiv = document.querySelector('#chart_div');

            options.width = window.innerWidth * 0.8;
            options.height = window.innerHeight * 0.5;

            chartDiv.style.width = options.width + 'px';
            chartDiv.style.height = options.height + 'px';

            chart.draw(dataTable, options);
        };
    </script>
</head>
<body>
    <!-- 현재 날짜와 시간을 표시할 요소 -->
    <div class="current-time" id="currentTime"></div>

    <p><b>실시간 조도, 온도 및 습도 변화</b></p>
    <div id="chart_div"></div>

    <script>
        function updateTime() {
            const now = new Date();

            // 날짜 (YYYY년 MM월 DD일 형식)
            const year = now.getFullYear();
            const month = String(now.getMonth() + 1).padStart(2, '0');
            const day = String(now.getDate()).padStart(2, '0');
            const formattedDate = `${year}년 ${month}월 ${day}일`;

            // 시간 (HH:MM:SS 형식)
            const hours = String(now.getHours()).padStart(2, '0');
            const minutes = String(now.getMinutes()).padStart(2, '0');
            const seconds = String(now.getSeconds()).padStart(2, '0');
            const formattedTime = `${hours}:${minutes}:${seconds}`;

            // 날짜와 시간을 함께 표시
            document.getElementById("currentTime").innerHTML = `${formattedDate}<br>${formattedTime}`;
        }

        // 최초 실행 및 1초마다 업데이트
        updateTime();
        setInterval(updateTime, 1000);
    </script>

</body>
</html>
