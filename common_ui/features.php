<?php
header("Content-Type: text/javascript");
echo $_GET['callback'] . '({"address":"http://localhost/esp8266/bedroom",app_name":"Bedroom local sim","is_powered":true,"on_url":"action.php?relay=true","off_url":"action.php?relay=false"});';
