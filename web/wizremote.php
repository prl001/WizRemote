<?php
//wizRemote php web-frontend
//Copyright 2008 Eric Fry
//This code is licensed under the GPLv2

//This is mainly a proof of concept. It is alpha software.
//Use at your own risk!
require_once('AES.class.php');

//wizRemote config ////////////////////////////////////////////////////////////////////

$GLOBALS["user_auth"] = TRUE;
$GLOBALS["userid"] = "userid";
$GLOBALS["passwd"] = "passwd";

$GLOBALS["aes_passphrase"] = "passphrase"; //this should match your wizremote.key file

$GLOBALS["wiz_ip"] = "localhost";
//$GLOBALS["wiz_ip"] = "192.168.1.102";
$GLOBALS["wiz_port"] = "30464";

$GLOBALS["timeout"] = "60";

//FIXME! I'd imagine these are specific to my service list.
$GLOBALS["channels"] = array(
 "4112,545,545" => "ABC TV Sydney",
 "12802,768,769" => "SBS",
"4116,1538,1573" => "Ten Digital");

//end config //////////////////////////////////////////////////////////////////////////

$GLOBALS["aes_key"] = md5($GLOBALS["aes_passphrase"]);
$GLOBALS["token_xor_key"] = array(0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0xba, 0xbe, 0xae, 0x57, 0x05e, 0x29, 0xfd, 0x87, 0x10, 0xe6);
 
//Used for converting to and from modified julian date format
$GLOBALS["base_date"] = gmmktime(0,0,0,1,1,2008); //unix timestamp 2008-01-01 GMT
$GLOBALS["wiz_base_date"] = 54466; //mjd equivalent

$GLOBALS["is_iphone"] = FALSE;

function iphone_check()
{
 if(strpos($_SERVER['HTTP_USER_AGENT'],"iPhone") !== FALSE)
	$GLOBALS["is_iphone"] = TRUE;
}

function is_iphone()
{
 return $GLOBALS["is_iphone"];
}

function print_error()
{
 if(isset($GLOBALS["error"]))
	echo "<b>Error:</b> {$GLOBALS["error"]}<br>";
}

function check_user()
{
 $response = "";

 if(isset($_REQUEST["logout"]))
	unset($_SESSION["userid"]);

 if(isset($_SESSION["userid"]))
	return TRUE;

 if(isset($_REQUEST["userid"]) && $_REQUEST["userid"] == $GLOBALS["userid"])
 {
	if(isset($_REQUEST["passwd"]) && $_REQUEST["passwd"] == $GLOBALS["passwd"])
	{
		$_SESSION["userid"] = $_REQUEST["userid"];
		return TRUE;
	}
 }
 if(isset($_REQUEST["userid"]))
 {
	$userid = $_REQUEST["userid"];
	$response = "Access Denied";
 }
 else
	$userid = "";

 render_header("");
 ?>

	<table border=0>
	<form method="POST">
		<tr>
			<td colspan=2><b>Login</b></td>
		</tr>
		<tr>
			<td colspan=2><font color="#ff0000"><?echo $response;?></font></td>
		</tr>
		<tr>
			<td>Username:</td>
			<td><input type="text" name="userid" id="userid" value="<?echo $userid; ?>"></td>
		</tr>
		<tr>
			<td>Password:</td>
			<td><input type="password" name="passwd" id="passwd"></td>
		</tr>
		<tr>
			<td colspan=2><input type="submit" name="submit" id="submit" value="Login"></td>
		</tr>
	</form>
	</table>
</body>
</html>
<?php
}

function wiz_connect()
{
 $wiz = @fsockopen($GLOBALS["wiz_ip"], $GLOBALS["wiz_port"], $errno, $errstr, $GLOBALS["timeout"]);
 if($wiz === FALSE)
 {
	$GLOBALS["error"] = "connecting to the beyonwiz. ($errstr)";
	return FALSE;
 }

 stream_set_timeout($wiz, $GLOBALS["timeout"]);

 wiz_aes_handshake($wiz);

 return  $wiz;
}

function wiz_aes_handshake($wiz)
{
 $enc = fread($wiz, 16);
 $strenc = bin2hex($enc);

 $aes = new AES();
 $token = $aes->decrypt($strenc, $GLOBALS["aes_key"]);

 $token_xor = "";
 for($i=0;$i<16;$i++)
 {
	$token_xor .= chr(hexdec(substr($token, $i*2, 2)) ^ $GLOBALS["token_xor_key"][$i]);
 }
 
 $response = $aes->encrypt(bin2hex($token_xor), $GLOBALS["aes_key"]);

 $response_bin = "";
 for($i=0;$i<16;$i++)
 {
	$response_bin .= chr(hexdec(substr($response, $i*2, 2)));
 }

 fwrite($wiz, $response_bin);

}

function wiz_read_response($wiz)
{
	$ret = fgets($wiz);
	if(trim($ret) != "ok")
	{
		$GLOBALS["error"] = "Bad response!\n";
		return FALSE;
	}
	
	return TRUE;
}

function wiz_close($wiz)
{
 fclose($wiz);
}

function render_wiz_connect_error()
{
 render_header("");
 ?>
 <h2>Error connecting to beyonwiz!</h2>
 <?php print_error(); ?>
 </body>
 </html>
 <?php
}

function decode_mjd($date)
{
 return $GLOBALS["base_date"] + (($date - $GLOBALS["wiz_base_date"]) * 86400);
}

function encode_mjd($date)
{
 return $GLOBALS["wiz_base_date"] + ($date - $GLOBALS["base_date"]) / 86400;
}

function render_header($meta)
{
?>
<html>
	<head>
<?
 if(is_iphone())
	echo "\t\t<meta name=\"viewport\" content=\"width = 320\" />\n";
 echo "\t\t$meta\n";
?>
		<script language="javascript" src="validation.js" />
	</head>
<body>
<?php
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

function render_timers($wiz)
{
 $page = $_SERVER['PHP_SELF'];

 fwrite($wiz,"list\n");
 
 $count = fgets($wiz);
 if($count > 0)
 {
	echo "<table border=1 cellspacing=0 cellpadding=2><tr><th>Filename</th><th>Start Date</th><th>Next Start</th>";
	echo "<th>Time</th><th>Len</th><th>Occur</th><th>Play</th><th>Lock</th><th>Ch</th><th>Action</th></tr>";
	for($i=0;$i<$count;$i++)
	{
		$line = fgets($wiz);
		list($fname,$startmjd,$nextmjd,$start,$duration,$repeat,$play,$lock,
			$onid,$tsid,$svcid) = split(":",trim($line));
		$startdate = decode_mjd($startmjd);
		$nextdate = decode_mjd($nextmjd);
		$starttime = date("g:ia",mktime(0,0,$start));

		$h = floor($duration / 3600);
		$m = floor(($duration % 3600) / 60); 
		if($h > 0)
			$duration_str = "{$h}h,&nbsp;";
		else
			$duration_str = "";
		
		$duration_str .= "{$m}m";
		
		$play = render_flag($play);
		$lock = render_flag($lock);
		$channel = "$onid,$tsid,$svcid";
		if(isset($GLOBALS["channels"][$channel]))
			$channel = $GLOBALS["channels"][$channel];

		$edit_data = urlencode("$fname:$startmjd,$start,$duration,$repeat,$onid,$tsid,$svcid");

		echo "<tr><td>$fname</td><td>".date("d-M-Y",$startdate)."</td><td>".date("d-M-Y",$nextdate)."</td>";
		echo "<td>$starttime</td><td>$duration_str</td><td>".render_repeat($repeat)."</td><td>$play</td><td>$lock</td><td>$channel</td>";
		echo "<td align=\"center\"><a href=\"$page?cmd=edit&data=$edit_data\"><img border=0 src=\"images/icon_edit_on.gif\"></a>";
		echo "&nbsp;/&nbsp;";
		echo "<a href=\"$page?cmd=delete&data=$startmjd,$start,$onid,$tsid,$svcid\" onClick=\"return confirm_delete('$fname')\"><img border=0 src=\"images/icon_delete_on.gif\"></a></td></tr>\n";
	}
	echo "</table>";
 }
 else
	echo "No Timers!";
 
 $ret = fgets($wiz);
 if(trim($ret) != "ok")
	$GLOBALS["error"] = "Getting timer list!\n";
}

function render_channel_dropdown($channel)
{
 $str = "<select name=\"channel\" id=\"channel\">";
 
 foreach ($GLOBALS["channels"] as $key => $value)
 {
    $str .="<option value=\"$key\" ";
	if($channel == $key)
		$str .= "selected ";
	$str .= ">$value</option>";
 }
 $str .= "</select>";
 
 return $str;
}

function render_start_field($datemjd)
{
 $months = array("Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec");
 
 $curYear = date("Y");
 
 if($datemjd != "")
 {
	$date = decode_mjd($datemjd);
	$day = gmdate("d",$date);
	$month = gmdate("m",$date);
	$year = gmdate("Y",$date);
 }
 else
 {
	$day = date("d");
	$month = date("n");
	$year = $curYear;
 }
 
 $str = "<select name=\"startDay\" id=\"startDay\">";
 for($i=1;$i<=31;$i++)
 {
	$str .= "<option value=\"$i\"";
	if($i==$day)
		$str .= " selected";
	$str .= ">$i</option>";
 }
 $str .= "</select>\n";
 

 $str .= "<select name=\"startMonth\" id=\"startMonth\">";
 for($i=1;$i<=12;$i++)
 {
	$str .= "<option value=\"$i\"";
	if($i==$month)
		$str .= " selected";
	$str .= ">{$months[$i-1]}</option>";
 }
 $str .= "</select>\n";

 $str .= "<select name=\"startYear\" id=\"startYear\">";
 for($i=$curYear;$i< $curYear + 2;$i++)
 {
	$str .= "<option value=\"$i\"";
	if($i==$year)
		$str .= " selected";
	$str .= ">$i</option>";
 }
 $str .= "</select>\n";

 return $str;
}

function render_start_time($starttime)
{
 if($starttime != "")
	$starttime = date("g:ia",mktime(0,0,$starttime));

 $str = "<input type=\"text\" name=\"start\" id=\"start\" size=\"7\" maxlength=\"7\" value=\"$starttime\">";
 return $str;
}

function render_duration($duration)
{
 if($duration != "")
 {
	$h = floor($duration / 3600);
	$m = floor(($duration % 3600) / 60); 
	if($h > 0)
		$duration = "{$h}h ";
	else
		$duration = "";
		
	$duration .= "{$m}";
 }
 
 $str = "<input type=\"text\" name=\"duration\" id=\"duration\" size=\"7\" value=\"$duration\">";
 return $str;
}

function render_occurrence($occurrence)
{
 $a = array(0 => "Once", 1 => "Daily", 2 => "Weekend", 3 => "Weekday", 4 => "Weekly");

 $str = "<select name=\"occurrence\" id=\"occurrence\">\n";
 
 foreach ($a as $key => $value)
 {
	$str .= "<option value=\"$key\" ";
	if($key == $occurrence)
		$str .= "selected ";
	
	$str .= ">$value</option>\n";
 }	
 $str .= "</select>\n";
 
 return $str;
}

function render_timer_input_form($type)
{
 if($type == "EDIT")
	echo "<h3>Edit Timer</h3>";
 else
	echo "<h3>New Timer</h3>";

 if($type == "EDIT")
 {
	list($fname, $data) = split(":", $_REQUEST["data"]);
	list($startmjd, $start, $duration, $repeat, $onid, $tsid, $svcid) = split(",", $data);
	$channel = "$onid,$tsid,$svcid";
	$data = "$startmjd,$start,$channel";
 }
 else
 {
	$fname = "";
	$startmjd = "";
	$start = "";
	$duration = "";
	$repeat = "0";
	$channel="";
 }

?>

<form method="POST" onSubmit="return check_timer();" action="<?php echo $_SERVER['PHP_SELF']; ?>">
<input type="hidden" name="type" value="<?php echo $type;?>">
<?php
if($type == "EDIT")
	echo "<input type=\"hidden\" name=\"data\" value=\"$data\">";
?>
	<table bgcolor="#dfdfdf">
		<tr><td align="right">Name:</td><td><input type="text" name="fname" id="fname" value="<?echo $fname;?>"size="35"></td></tr>
		<tr><td align="right">Channel:</td><td><?php echo render_channel_dropdown($channel); ?></td></tr>
		<tr><td align="right">Date:</td><td><?php echo render_start_field($startmjd); ?></td></tr>
		<tr><td align="right">Time:</td><td><?php echo render_start_time($start); ?>&nbsp;eg ( 4:25pm or 16:25 )</td></tr>
		<tr><td align="right">Duration:</td><td><?php echo render_duration($duration); ?>&nbsp;eg ( 70 or 1h10 )</td></tr>
		<tr><td align="right">Occurrence:</td><td><?php echo render_occurrence($repeat); ?></td></tr>
		<tr><td colspan="2">&nbsp;</td></tr>
		<tr><td colspan="2"><input type="submit" name="submit" id="submit" value="  <?php echo $type == "ADD" ? "Add" : "Update";?> Timer  ">
<?php if($type == "EDIT")
 {
?>
	<input type="button" value="  Cancel Update  " onClick="window.location='<?echo $_SERVER['PHP_SELF'];?>'">
<?php
 }
?>
</td></tr>
	</table>
</form>
<?php
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

function add_timer($wiz)
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

	$addstr = "add\n$fname:$startmjd:$nextmjd:$start:$duration:$occurrence:0:0:$channel\n";
	//echo "<pre>$addstr</pre>";
	fwrite($wiz, $addstr);

	return wiz_read_response($wiz);
}

function edit_timer($wiz)
{
 $data = $_REQUEST["data"];
 if(delete_timer($wiz, $data))
 {
	add_timer($wiz);
 }
}

function delete_timer($wiz, $data)
{
	$timer = str_replace(",",":",$data);
	if(!preg_match("/^([0-9]+:){4}[0-9]+$/",$timer))
	{
		echo "Invalid timer data!";
		return FALSE;
	}

	$cmd = "delete\n$timer\n";
	fwrite($wiz, $cmd);
	
	return wiz_read_response($wiz);
}

function save_and_reboot($wiz)
{
 $addstr = "reboot\n";
 fwrite($wiz, $addstr);

 render_header("<meta http-equiv=refresh content=\"25;url={$_SERVER['SCRIPT_NAME']}\">");
?>

	<h2>Rebooting Unit.</h2>
	<table border=0><tr><td><img src="images/loading.gif"></td><td><b>Please Wait....</b></td></tr></table>
</body>
</html>
	<?php
}


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

$input_type = "ADD";

if(isset($_REQUEST["cmd"]))
{
	$data = isset($_REQUEST["data"]) ? $_REQUEST["data"] : "";
	
	switch($_REQUEST["cmd"])
	{
		case "edit"   : $input_type="EDIT"; break;
		case "delete" :
			delete_timer($wiz, $data);
			wiz_close($wiz); 
			header('Location: '.$_SERVER['PHP_SELF']);
			exit();
			break;
		case "reboot" :
			save_and_reboot($wiz);
			wiz_close($wiz);
			exit();
			break;
	}
}

//FIXME we need more validation on input data.
if($_SERVER["REQUEST_METHOD"] == "POST")
{
	switch($_REQUEST["type"])
	{
		case "ADD" : add_timer($wiz); break;
		case "EDIT" : edit_timer($wiz); break;
		default : break;
	}
}

render_header("");
?>

<h1>WizRemote</h1>
<table cellpadding=0 cellspacing=0 border=0>
<tr>
<td>
This is a sample application for setting timers remotely on your Beyonwiz PVR!<p>
Created by: <b>Eric Fry</b>
<p>
<p>
</td>
</tr>
<tr>
<td>
<h3>Current Timers</h3>
<?php
render_timers($wiz);
render_timer_input_form($input_type);
?>
<br>
<br>
</td>
</tr>
<tr>
<td>
<hr noshade size=1 width="100%">
</td>
</tr>
<tr>
<td>
<form><input type="button" value="  Save and Reboot  " onClick="window.location='?cmd=reboot'">&nbsp;
<?php if(isset($GLOBALS["user_auth"])) echo "<input type=\"button\" value=\"  Logout  \" onClick=\"window.location='?logout=1'\">"; ?>
</form>
</td>
</tr>
</table>
</body>
</html>
<?php
wiz_close($wiz);
?>