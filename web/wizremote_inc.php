<?php
require_once('AES.class.php');

require_once('config_inc.php');

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

function strip_magic_slashes($str)
{
 return get_magic_quotes_gpc() ? stripslashes($str) : $str;
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
		<script type="text/javascript" src="validation.js"></script> 
	</head>
<body>
<?php
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

?>