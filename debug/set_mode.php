<?php
  $ref_file = 'features_' . $_GET['mode'] . '.json';
  if(file_exists($ref_file)){
  	copy($ref_file, '../common_ui/features.json');
  	echo '<h1>mode now ' . $_GET['mode'] . '</h1>';
  }else{
		echo '<h1>Missing mode param, please use:</h1>';
  }

  $modes = ['percentage','momentary','toggle'];
  $local_store_entries = [];
	?>
	<ul>
		<?php for($i = 0; $i < sizeof($modes);$i++){
			array_push($local_store_entries,preg_replace( "/\r|\n/", "", file_get_contents('features_' . $modes[$i] . '.json')));
		  ?>
		<li><a href="?mode=<?php echo $modes[$i]?>"><?php echo $modes[$i]?></a></li>
		<?php } ?>
	</ul>

	Local store entry is:
	<textarea>[<?php echo join(',',$local_store_entries);?>]</textarea>

