<?php
//wizRemote php web-frontend
//Copyright 2008 Eric Fry
//This code is licensed under the GPLv2

//This is mainly a proof of concept. It is alpha software.
//Use at your own risk!


//wizRemote config

$GLOBALS["wiz_ip"] = "localhost";
//$GLOBALS["wiz_ip"] = "192.168.1.102";
$GLOBALS["wiz_port"] = "30464";

$GLOBALS["timeout"] = "60";

//FIXME! I'd imagine these are specific to my service list.
$GLOBALS["channels"] = array(
 "4112,545,545" => "ABC TV Sydney",
 "12802,768,769" => "SBS",
"4116,1538,1573" => "Ten Digital");

//end config


//Used for converting to and from modified julian date format
$GLOBALS["base_date"] = gmmktime(0,0,0,1,1,2008); //unix timestamp 2008-01-01 GMT
$GLOBALS["wiz_base_date"] = 54466; //mjd equivalent

function print_error()
{
 if(isset($GLOBALS["error"]))
	echo "<b>Error:</b> {$GLOBALS["error"]}<br>";
}

function wiz_connect()
{
 $wiz = @fsockopen($GLOBALS["wiz_ip"], $GLOBALS["wiz_port"], &$errno, &$errstr, $GLOBALS["timeout"]);
 if($wiz === FALSE)
 {
	$GLOBALS["error"] = "connecting to the beyonwiz. ($errstr)";
	return FALSE;
 }
 
 stream_set_timeout($wiz, $GLOBALS["timeout"]);
 
 return  $wiz;
}

function wiz_close($wiz)
{
 fclose($wiz);
}

function decode_mjd($date)
{
 return $GLOBALS["base_date"] + (($date - $GLOBALS["wiz_base_date"]) * 86400);
}

function encode_mjd($date)
{
 return $GLOBALS["wiz_base_date"] + ($date - $GLOBALS["base_date"]) / 86400;
}

function render_repeat($repeat)
{
 switch($repeat)
 {
	case 0 : $repeat = "Once"; break;
	case 1 : $repeat = "Daily"; break;
	case 2 : $repeat = "Weekend"; break;
	case 3 : $repeat = "Weekday"; break;
	case 4 : $repeat = "Weekly"; break;
	default : break;
 }

 return $repeat;
}

function render_flag($flag)
{
 return $flag ? "Y" : "N";
}

function render_timers()
{
 $wiz = wiz_connect();
 if($wiz === FALSE)
 {
	print_error();
	return FALSE;
 }

 fwrite($wiz,"list\n");
 
 $count = fgets($wiz);
 if($count > 0)
 {
	echo "<table border=1 cellspacing=0 cellpadding=2><tr><th>Filename</th><th>Start Date</th><th>Next Start Time</th>";
	echo "<th>Start Time</th><th>Recording Duration</th><th>Occurrence</th><th>Play</th><th>Lock</th><th>Channel</th></tr>";
	for($i=0;$i<$count;$i++)
	{
		$line = fgets($wiz);
		list($fname,$startmjd,$nextmjd,$start,$duration,$repeat,$play,$lock,
			$onid,$tsid,$svcid) = split(":",trim($line));
		$startmjd = decode_mjd($startmjd);
		$nextmjd = decode_mjd($nextmjd);
		$start = date("g:ia",mktime(0,0,$start));

		$h = floor($duration / 3600);
		$m = floor(($duration % 3600) / 60); 
		if($h > 0)
			$duration = "{$h}h,&nbsp;";
		else
			$duration = "";
		
		$duration .= "{$m}m";
		
		$repeat = render_repeat($repeat);
		$play = render_flag($play);
		$lock = render_flag($lock);
		$channel = "$onid,$tsid,$svcid";
		if(isset($GLOBALS["channels"][$channel]))
			$channel = $GLOBALS["channels"][$channel];

		echo "<tr><td>$fname</td><td>".date("d-M-Y",$startmjd)."</td><td>".date("d-M-Y",$nextmjd)."</td>";
		echo "<td>$start</td><td>$duration</td><td>$repeat</td><td>$play</td><td>$lock</td><td>$channel</td></tr>";
	}
	echo "</table>";
 }
 wiz_close($wiz);
}

function render_channel_dropdown()
{
 $str = "<select name=\"channel\" id=\"channel\">";
 
 foreach ($GLOBALS["channels"] as $key => $value)
 {
    $str .="<option value=\"$key\">$value</option>\n";
 }
 $str .= "</select>";
 
 return $str;
}

function render_start_field()
{
 $months = array("Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec");
 $curDay = date("d");
 $curMonth = date("n");
 $curYear = date("Y");

 $str .= "<select name=\"startDay\" id=\"startDay\">";
 for($i=1;$i<=31;$i++)
 {
	$str .= "<option value=\"$i\"";
	if($i==$curDay)
		$str .= " selected";
	$str .= ">$i</option>";
 }
 $str .= "</select>\n";
 

 $str .= "<select name=\"startMonth\" id=\"startMonth\">";
 for($i=1;$i<=12;$i++)
 {
	$str .= "<option value=\"$i\"";
	if($i==$curMonth)
		$str .= " selected";
	$str .= ">{$months[$i-1]}</option>";
 }
 $str .= "</select>\n";

 $str .= "<select name=\"startYear\" id=\"startYear\">";
 for($i=$curYear;$i< $curYear + 2;$i++)
 {
	$str .= "<option value=\"$i\"";
	if($i==$curYear)
		$str .= " selected";
	$str .= ">$i</option>";
 }
 $str .= "</select>\n";

 return $str;
}

function parse_start($s)
{
 //sanitise the time string.
 $s = str_replace(" ", "", $s);
 $s = str_replace("am","", $s, $am_flag);

 //wipe pm string set pm_flag if string found
 $s = str_replace("pm", "", $s, $pm_flag);
 
 list($h,$m) = split(":", $s);

 if($h == 12 && $am_flag) //12am is 0-hour in 24 hour clock
	$h = 0;

 if($h < 12 && $pm_flag) //convert 12 hour time to 24 hour time
	$h += 12;

 return ($h * 60 + $m) * 60;
}

function parse_duration($d)
{
 $d = str_replace(" ", "", $d);
 $d = str_replace(":", "h", $d);

 if(preg_match("/^[0-9]+h[0-9]*$/i", $d) == 1)
 {
	list($h,$m) = split("h", strtolower($d));
	$d = $h * 60 + $m;
 }

 $d = $d * 60; //convert minutes to seconds

 return $d;
}

//FIXME we need more validation on input data.
if($_SERVER["REQUEST_METHOD"] == "POST")
{
 $fname = trim(str_replace(":", " ", $_REQUEST["fname"]));

 $startdate = gmmktime(0,0,0,$_REQUEST["startMonth"],$_REQUEST["startDay"],$_REQUEST["startYear"]);

 $occurrence = $_REQUEST["occurrence"];
 if($occurrence < 0 || $occurrence > 4)
	$occurrence = 0;

 $startmjd = encode_mjd($startdate);
 $nextmjd = $startmjd;
 
 $start = parse_start($_REQUEST["start"]);
 $duration = parse_duration($_REQUEST["duration"]);

 $channel = $_REQUEST["channel"];
 if(preg_match("/^[0-9]+,[0-9]+,[0-9]+$/",$channel) == 0)
 {
	$channel = "";
	echo "Error";
 }
 
 $channel = str_replace(",", ":", $channel);
 
 $wiz = wiz_connect();
 if($wiz !== FALSE)
 {
	$addstr = "add\n$fname:$startmjd:$nextmjd:$start:$duration:$occurrence:0:0:$channel\n";
	//echo "<pre>$addstr</pre>";
	fwrite($wiz, $addstr);
	fclose($wiz);
	?>
	<html>
	<head>
	<meta http-equiv=refresh content=25>
	</head>
	<body>
	<h2>Rebooting Unit.</h2>
	<table border=0><tr><td><img src="loading.gif"></td><td><b>Please Wait....</b></td></tr></table>
	</body>
	</html>
	<?php
	exit();
 }
  
}

?>
<html>
<head>
<script language="javascript" src="validation.js">
</head>
<body>
<h1>WizRemote</h1>
This is a sample application for setting timers remotely on your Beyonwiz PVR!<p>
Created by: <b>Eric Fry</b>
<p>
<p>
<h3>Current Timers</h3>
<?php
render_timers();
?>
<h3>New Timer</h3>
<form method="POST" onSubmit="return check_timer();">
<table bgcolor="#dfdfdf">
<tr><td align="right">Name:</td><td><input type="text" name="fname" id="fname" size="35"></td></tr>
<tr><td align="right">Channel:</td><td><?php echo render_channel_dropdown(); ?></td></tr>
<tr><td align="right">Date:</td><td><?php echo render_start_field(); ?></td></tr>
<tr><td align="right">Time:</td><td><input type="text" name="start" id="start" size="7" maxlength="7">&nbsp;eg ( 4:25pm or 16:25 )</td></tr>
<tr><td align="right">Duration:</td><td><input type="text" name="duration" id="duration" size="7">&nbsp;eg ( 70 or 1h10 )</td></tr>
<tr><td align="right">Occurrence:</td><td>
<select name="occurrence" id="occurrence">
	<option value="0">Once</option>
	<option value="1">Daily</option>
	<option value="2">Weekend</option>
	<option value="3">Weekday</option>
	<option value="4">Weekly</option>
</select></td></tr>
<tr><td colspan="2">&nbsp;</td></tr>
<tr><td colspan="2"><input type="submit" name="addTimer" id="addTimer" value="  Add Timer  "></td></tr>
</table>

</form>
</body>
</html>