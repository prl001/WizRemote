function trim(str)
{
	return str.replace(/^\s\s*/, '').replace(/\s\s*$/, '');
}

function urldecode(str)
{
	str = str.replace(/\+/g, ' ');
	str = unescape(str);

	return str;
}

function get_form_element(form_id)
{
 var item = document.getElementById(form_id);

 return item;
}

function date_validate(strDate)
{
 var regexp = /^[ \t]*[0-9]{1,2}\/[0-9]{1,2}\/[0-9]{4}[ \t]*$/;
 if(!strDate.match(regexp))
   return false;

 var date = strDate.split("/");

 if(date.length < 3)
   return false;

 year = parseInt(date[2],10)
 month = parseInt(date[1],10);
 day = parseInt(date[0],10);

 if(isNaN(year) || isNaN(month) || isNaN(day))
   return false;

 if (month < 1 || month > 12)
   return false;
 if (day < 1 || day > 31)
   return false;

 if ((4 == month || 6 == month || 9 == month || 11 == month) && (31 == day))
   return false;

 if (2 == month)
   {
     if ((0 == year % 4) && ((0 != year % 100) || (0 == year % 400)))
       {
         if(day > 29)
            return false;
       }
     else
       {
         if(day > 28)
            return false;
       }
   }

  return true;
}

function time_validate(strTime)
{
	var twelvehour = false;
	strTime = trim(strTime);
	
	var strTimeOrig = strTime;
	
	if(strTime == "")
	{
		alert("Please enter a start time!");
		return false;
	}
	
	var regexp = /^[0-9]{1,2}:[0-9]{2}(am|pm|)$/i;
	if(regexp.test(strTime))
	{
		regexp = /am|pm/i;
		if(regexp.test(strTime))
		{
			var strTime = strTime.replace(/am|pm/i,"");
			twelvehour = true;
		}
	
		var data = strTime.split(":");
		hour = parseInt(data[0],10);
		minute = parseInt(data[1],10);
	
		if(hour <= 23 && minute <= 59)
		{
			if(twelvehour  == false || (hour <= 12 && hour != 0))
				return true;
		}
	}

	alert("Invalid start time! '" + strTimeOrig + "'");
	return false;
}

function duration_validate(strDuration)
{
	strDuration = strDuration.replace(/[ \t]*/g,"");
	
	if(strDuration == "")
	{
		alert("Please enter a duration!");
		return false;
	}

	var regexp = /^[0-9]*$|^[0-9]+(h|:)[0-9]*$/i;

	if(regexp.test(strDuration) == false)
	{
		alert("Invalid Duration! '" + strDuration + "'");
		return false;
	}
	
	return true;
}

function name_validate(strName)
{
	strName = trim(strName);
	
	if(strName == "")
	{
		alert("Please enter a name!");
		return false;
	}
/*
	var regexp = /^[0-9,a-z,_\- ]+$/i;
	
	if(regexp.test(strName) == false)
	{
		alert("Invalid Name!");
		return false;
	}
*/
	return true;
}

function check_timer()
{
	var name = get_form_element("fname");
	var start = get_form_element("start");
	var duration = get_form_element("duration");

	var startDay = get_form_element("startDay");
	var startMonth = get_form_element("startMonth");
	var startYear = get_form_element("startYear");

	var date = startDay.value + "/" + startMonth.value + "/" + startYear.value;
	

	if(name_validate(name.value) == false)
	{
		name.focus();
		return false;
	}
	
	if(date_validate(date) == false)
	{
		alert("Invalid date '" + date + "'");
		startDay.focus();
		return false;
	}

	if(time_validate(start.value) == false)
	{
		start.focus();
		return false;
	}


	if(duration_validate(duration.value) == false)
	{
		duration.focus();
		return false;
	}

	return  true;
}

function confirm_delete(name)
{
	return confirm("Are you sure you want to delete the timer named\n '" + urldecode(name) + "'?");
	//document.window.location = "?delete=" + data;
	//return;
}

