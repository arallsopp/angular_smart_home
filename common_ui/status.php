<?php
/* echo time of day. dst value. time to next feed. version number */
$mode = "percentage";
$mode = "momentary";
$mode = "normal";

sleep(1);
$obj = new stdClass();

if($mode == "percentage") {
	$obj->app_name = "Sample Fader Device";
}else if($mode == "momentary") {
	$obj->app_name = "Momentary";
}else{
	$obj->app_name = "Sample Toggle Device";
}
$obj->app_version="27.0";
$obj->time_of_day = date('H:i:s');
$obj->is_powered=true;

$obj->request = new stdClass();
$obj->request->base_url     = "action.php";
$obj->request->master_param = "master";

if($mode == "percentage") {
	$obj->request->start_param = "start";
	$obj->request->end_param  = "end";
	$obj->request->time_param = "duration";

	$obj->percentage = 100;
	$obj->perc_label = "Brightness";
}

$obj->is_dst = false;
$obj->is_using_timer= true;
$obj->next_event_due = 135;
$obj->is_skipping_next=false;
$obj->last_action="local simulation started in " + $mode + " mode.";

$obj->events = array();

for($i=0;$i<5;$i++) {
	$event = new stdClass();
	$event->time = (6 + $i) . ":30";
	$event->label = 'event number ' . ($i+1);
	$event->enacted = ($i<3);
	array_push($obj->events,$event);
}

echo json_encode($obj);
