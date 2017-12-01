<?php
  $ref_file = 'features_' . $_GET['mode'] . '.json';
  if(file_exists($ref_file)){
  	copy($ref_file, '../common_ui/features.json');
  	echo '<h1>mode now ' . $_GET['mode'] . '</h1>';
  }else{
		echo '<h1>Missing mode param, please use:</h1>';
  }

	?>
	<ul>
		<li><a href="?mode=percentage">percentage</a></li>
		<li><a href="?mode=momentary">momentary</a></li>
		<li><a href="?mode=normal">normal</a></li>
	</ul>
