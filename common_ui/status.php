<?php
/* echo time of day. dst value. time to next feed. version number */
session_start();
sleep(1);
$obj = new stdClass();
$obj->time_of_day = date('H:i:s');
$obj->is_dst = "false";
$obj->next_event_due = 135;
$obj->skipping_next=false;
$obj->app_name="Bedroom Lamp";
$obj->app_version="v27.0";
$obj->last_action="local simulation started";
$obj->events = array();

for($i=0;$i<5;$i++) {
	$event = new stdClass();
	$event->time = (6 + $i) . ":30";
	$event->label = 'event number ' . ($i+1);
	$event->enacted = ($i<3);
	array_push($obj->events,$event);
}

echo json_encode($obj);
