<?php

$copyright = '
// Generated by YateBTS NIB WebGUI
// Please don\'t edit manually

/**
 * subscribers.js
 * Configure subscribers individually or set regular expression to accept imsis
 *
 * This file is part of the Yate-BTS Project http://www.yatebts.com
 *
 * Copyright (C) 2014 Null Team
 *
 * This software is distributed under multiple licenses;
 * see the COPYING file in the main directory for licensing
 * information for this specific distribution.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

';

$subscribers_prefix = $copyright.'// Note! Subscribers are accepted by either matching the IMSI against a configured regular expression 
// or by individually configuring each acceptable IMSI

var subscribers = {';

$subscribers_suffix = '};

// Note! If a regular expression is used, 2G/3G authentication cannot be used. 
// To use 2G/3G authentication, please set subscribers individually.
//var regexp = /^001/;';

$regexp_prefix = $copyright.'/*
// Note! Subscribers are accepted by either matching the IMSI against a configured regular expression 
// or by individually configuring each acceptable IMSI

var subscribers = {
"001990010001014":
{
    "msisdn":"074434", // oficial phone number
    "active":1,
    "ki":"00112233445566778899aabbccddeeff",
    "op":"00000000000000000000000000000000",
    "imsi_type":"3G",
    "short_number":"" //short number to be called by other yatebts-nib users
},
"001990010001015":
{
    "msisdn":"074434", // oficial phone number
    "active":1,
    "ki":"00112233445566778899aabbccddeeff",
    "op":"",
    "imsi_type":"2G",
    "short_number":"" //short number to be called by other yatebts-nib users
}
};
*/

// Note! If a regular expression is used, 2G/3G authentication cannot be used. 
// To use 2G/3G authentication, please set subscribers individually.
var regexp = ';

$regexp_suffix = ";";

$dirs = array("/etc/yate/", "/usr/local/etc/yate/");
foreach ($dirs as $pos_dir) {
	if (is_dir($pos_dir))
		$yate_conf_dir = $pos_dir;
}
if (!isset($yate_conf_dir)) 
	print ("Couldn't detect installed yate. Please install yate first before trying to use the NIB WebGui.");

$yate_ip = "127.0.0.1";

$proj_title = "Yatebts NIB";

?>
