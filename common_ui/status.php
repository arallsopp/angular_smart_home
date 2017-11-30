<?php
/* echo time of day. dst value. time to next feed. version number */
$supporting_percentage = false;

session_start();
sleep(1);
$obj = new stdClass();
$obj->time_of_day = date('H:i:s');
$obj->is_dst = "false";
$obj->next_event_due = 135;
$obj->is_skipping_next=false;
$obj->is_powered=true;

$obj->request = new stdClass();
$obj->request->base_url     = "action.php";
$obj->request->master_param = "master";

if($supporting_percentage) {
	$obj->request->start_param = "start";
	$obj->request->end_param  = "end";
	$obj->request->time_param = "duration";

	$obj->percentage = 100;
	$obj->perc_label = "Brightness";
}

if($supporting_percentage) {
	$obj->app_name = "Sample Fader Device";
}else{
	$obj->app_name = "Sample Toggle Device";
}
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
