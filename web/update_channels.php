<?php

require_once('wizremote_inc.php');

//Start page processing.

iphone_check();

if($GLOBALS["user_auth"] == TRUE)
{
	session_start();
	if(check_user() == FALSE)
		exit();
}

$wiz = wiz_connect();
if($wiz == FALSE)
{
	render_wiz_connect_error();
	exit();
}

fwrite($wiz, "chlist\n");
$count = fgets($wiz);

$str = "<?php //Generated ".date("d-M-Y H:i:s")."\n\n";

$str .= "\$GLOBALS[\"channels\"] = array(\n";

for($i=0; $i < $count; $i++)
{
	list($data,$name) = split(":", fgets($wiz), 2);
	$name = trim($name);
	if($i>0)
		$str .= ",\n";
	$str .= "\"$data\" => \"$name\"";
}

$str .= ");\n\n?>";

wiz_read_response($wiz);
wiz_close($wiz);

header('Content-type: text/plain');

$fp = @fopen("config_channels_inc.php", "w");
if($fp)
{
	fwrite($fp, $str);
	fclose($fp);
	echo "New channel list saved to config_channels_inc.php\n\n";
	echo $str;
	exit();
}

header('Content-Disposition: attachment; filename="config_channels_inc.php"');

echo $str;
?>