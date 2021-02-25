// Kevin Gilboy
// kevingilboy@pitt.edu
// Pitt Makerspace - Aug 2018
// Makerspace server app for communicating with ESP8266-connected RFID readers and tool power sources over MQTT
// Includes support for an admin-based buddy system and sending emails when a tool times out

var express = require('express');
var app = express();
var bodyParser = require('body-parser');
var path = require('path');
var request = require('request');
var mqtt = require('mqtt');
var client = mqtt.connect('mqtt://localhost');
var nodemailer = require('nodemailer');

/// Transporter for email service, use the correct username/password below
var transporter = nodemailer.createTransport({
	service : 'gmail',
	auth    : {
		user : 'OUR EMAIL ADDRESS', //TODO
		pass : 'OUR EMAIL PASSWORD' //TODO
	}
});

/// IP address of our user database
var db_ip = 'OUR DATABASE IP'; //TODO

/// HTTP Authentication object for the database
var db_auth = {
	'user'            : 'OUR DATABASE USER ACCOUNT', //TODO
	'pass'            : 'OUR DATABASE PASSWORD', //TODO
	'sendImmediately' : false
}

/// Object for storing admin state
var admin = {
	present:false,
	name:null,
	uid:null,
	pitt_id:null,
	room:null
}

/// Timeout object that counts down once an admin leaves
/// Used to make switching between admins easier without turning off the tools
var admin_left_timeout;

/// Time between an admin card being removed and tools requiring a present admin turning off
/// This is to prevent all tools from disabling when admins switch cards during shift change
var delay_time_after_admin_leaves_ms = 5000;

/// If the database is unreachable, default to allow tools to turn on when any user puts an RFID-enabled ID in the holster
var tool_behavior_when_db_down = 'on';

/// Collection tracking what tools need to be enabled that require admin presence
var tools_queue_req_admin = [];

/// Collection tracking what tools are enabled that require admin presence
var tools_running_req_admin = [];

/// Collection tracking what tools are enabled that do not require admin presence
var tools_running_no_req_admin = [];

// view engine setup
app.use(express.static(path.join(__dirname, 'public')));
app.use(bodyParser.json());

// Connect to the MQTT broker and subscript to the relevant topics
client.on('connect', function() {
	client.subscribe([
		'makermqtt/rfid/admin_card_detected',
		'makermqtt/rfid/admin_card_removed',
		'makermqtt/rfid/tool_card_detected',
		'makermqtt/rfid/tool_card_removed',
		'makermqtt/rfid/card_timeout'
	]);
});

/// This architecture is message (i.e. event) driven. It is kicked off by an MQTT message being published to a topic we are subscribed to
client.on('message', function(topic, rawmsg) {
	console.log(topic.toString() + " says " + rawmsg.toString());

	// Parse the MQTT message into a JSON object, returning if not in valid JSON format
	var msg;
	try {
		msg = JSON.parse(rawmsg.toString());
	}
	catch(e) {
		return;
	}

	// If msg was received on the tool_card_detected topic and the tool is not on
	if(topic == "makermqtt/rfid/tool_card_detected" && msg.tool_on == "0") {
		// Ask the database for the user's info based on the UID read from their RFID card
		request.get(db_ip + '/db/users/getMetadataByUid', {
			'auth' : db_auth,
			'form' : {
				uid:msg.uid
			}
		}, function(err, res, body) {
			// If we fail, execute the default behavior when the DB is down as defined by the constant above
			if(err || body == null) {
				console.log("Database is down or body is null! :(");
				client.publish('makermqtt/' + msg.toolname,tool_behavior_when_db_down);
				return;
			}

			try{
				// Parse the response
				body = JSON.parse(body);

				// If the response says the user is trained, start a tool usage session for that tool
				if(body.training.indexOf(msg.toolname.replace(/ *\([^)]*\) */g,'')) != -1) {
					session_data = {
						toolname  : msg.toolname,
						uid       : msg.uid,
						req_admin : msg.req_admin,
						name      : body.name,
						pitt_id   : body.pitt_id,
						tool_on   : msg.tool_on
					}
					StartToolSession(session_data);
				}
				// Else, they are not trained, so make sure tool is off
				else {
					client.publish('makermqtt/' + msg.toolname,'off');
				}
			}
			catch(e) {
				// In any error, make sure the tool is off
				client.publish('makermqtt/' + msg.toolname,'off');
			}
		});
	}
	// Else if msg was received on the tool_card_removed topic, end the tool usage session for that tool
	else if(topic == 'makermqtt/rfid/tool_card_removed') {
		session_data = {
			toolname  : msg.toolname,
			uid       : msg.uid,
			req_admin : msg.req_admin,
			tool_on   : msg.tool_on
		}
		EndToolSession(session_data);
	}
	// Else if msg was received on the card_timeout topic
	else if(topic == 'makermqtt/rfid/card_timeout') {
		// Ask the database for the user's info based on the UID read from their RFID card
		request.get(db_ip + '/db/users/getMetadataByUid', {
			'auth' : db_auth,
			'form' : {
				uid:msg.uid
			}
		}, function(err,res,body){
			// We cannot do anything if the DB is down, so return
			if(err || body == null){
				console.log("Database is down or body is null! :(");
				return;
			}

			try {
				// Parse the response
				body = JSON.parse(body);

				// Create an email body to send to the user's email, parsed from the DB response body
				var mailOptions = {
					from: 'pittmakerspace@gmail.com',
					to: body.pitt_id+'@pitt.edu',
					subject: 'Pitt ID Left in Makerspace Tool?',
					text: body.name+':\n\nOur RFID system has detected that you possibly left your Pitt ID in the Makerspace tool "'+msg.toolname+'" since activity has not been sensed for 10 minutes. Please secure your ID and be careful to only leave it in the machine while you are operating it.\n\nThank you,\nThe Makerspace Team at makerspace@pitt.edu'
				};

				// Send the email
				transporter.sendMail(mailOptions, function(err, info) {
					if (err) {
						console.log('Email error: ' + err);
					}
					else {
						console.log('Email sent: ' + info.response);
					}
				});
			}
			catch(e) {
				return;
			}
		});
	}
	// Else if msg was received on the admin_card_detected topic
	else if(topic == "makermqtt/rfid/admin_card_detected") {
		// Ask the database for the admin's info based on the UID read from their RFID card
		request.get(db_ip + '/db/users/getMetadataByUid', {
			'auth' : db_auth,
			'form' : {
				uid:msg.uid
			}
		}, function(err, res, body) {
			// If the DB is down, we assume there is no admin in the room for safety reasons
			if(err || body == null) {
				admin.present = false;
				console.log("Database is down or body is null! :(");
				return;
			}

			try {
				// Parse the response
				body = JSON.parse(body);

				// If the response says the user is an admin, start an admin session
				if(body.training.indexOf(msg.toolname.replace(/ *\([^)]*\) */g,'')) != -1) {
					admin.name    = body.name;
					admin.uid     = msg.uid;
					admin.pitt_id = body.pitt_id;
					admin.room    = msg.toolname;

					AdminArrival();
				}
				// Else, they are an imposter (i.e. not an admin)
				else {
					admin.present = false;
				}
			}
			catch(e) {
				admin.present = false;
			}
		});
	}
	// Else if the msg was received on the admin_card_removed topic, end the admin session
	else if(topic == 'makermqtt/rfid/admin_card_removed') {
		AdminDeparture();
	}

	// After handling the message, print out the result of the buffer collections to show the state of the room
	setTimeout(function() {
		console.log("Running(no-admin):" + tools_running_no_req_admin.length);
		console.log("Running(admin):" + tools_running_req_admin.length);
		console.log("Queue(admin):" + tools_queue_req_admin.length);
		console.log("");
	}, 500);
});

/// Starts a tool session by turning the tool on and posting to the DB to log the session
/// The user has already been deemed as "trained" here
function StartToolSession(session_data) {
	// Flag the tool as being turned on
	session_data.tool_on = "1";

	// If the tool requires admin presence
	if(session_data.req_admin) {
		// Case: User wants to use machine and has their ID on the machine, but there is no admin
		// Result: Push to tool queue waiting for admin arrival
		if(!admin.present) {
			RemoveToolByToolName(tools_queue_req_admin, session_data.toolname)
			tools_queue_req_admin.push(session_data);

			// Do not turn the tool on
			return;
		}
		// Case: User wants to use machine and has their ID on the machine, and yay an admin is present
		// Result: push to tool running list that requires admin
		else if(admin.present) {
			// Ensure the tool is not already in the collection (this should never be the case but it prevents duplicate records)
			RemoveToolByToolName(tools_running_req_admin, session_data.toolname)
			tools_running_req_admin.push(session_data);
		}
	}
	// Else the tool does not require admin presence
	else {
		// Ensure the tool is not already in the collection (this should never be the case but it prevents duplicate records)
		RemoveToolByToolName(tools_running_no_req_admin, session_data.toolname)
		tools_running_no_req_admin.push(session_data);
	}

	// Add the admin info to the session name, which will be logged in the DB along with the tool session
	session_data.admin_name    = admin.name;
	session_data.admin_uid     = admin.uid;
	session_data.admin_pitt_id = admin.pitt_id;

	// Turn on the tool via MQTT publish on the toolname topic
	client.publish('makermqtt/' + session_data.toolname, 'on');

	// POST the session to the DB for logging
	request.post(db_ip+'/db/tools/startsession', {
		'auth' : db_auth,
		'form' : session_data
	}, function(err, res, body) {
		// Do nothing on response
	});
}

/// Ends a tool session by turning the tool off and posting to the DB to log the session
function EndToolSession(session_data){
	// If the tool requires admin presence
	if(session_data.req_admin) {
		// Case: User removes card and was either not trained or there was no admin in the room so the tool never turned on
		// Result: Remove from queue and do not post to DB
		if(session_data.tool_on == "0") {
			RemoveToolByToolName(tools_queue_req_admin, session_data.toolname)

			// No need to turn off since it was already off
			return;
		}
		// Case: The user has not removed their ID from the machine but the admin left
		// Result: Remove from tool running list and push to tool queue waiting for admin arrival
		else if(!admin.present) {
			RemoveToolByToolName(tools_running_req_admin, session_data.toolname)
			session_data.tool_on = "0";
			tools_queue_req_admin.push(session_data);
		}
		// Case: User chooses to leave and removes their ID from the machine while the admin is still present
		// Result: Remove from tool running list that requires admin
		else if(admin.present) {
			RemoveToolByToolName(tools_running_req_admin, session_data.toolname)
		}
	}
	// Else the tool does not require admin presence
	else{
		RemoveToolByToolName(tools_running_no_req_admin, session_data.toolname)
	}

	// Turn off the tool via MQTT publish on the toolname topic
	client.publish('makermqtt/' + session_data.toolname, 'off');

	// POST the session to the DB for logging
	request.post(db_ip + '/db/tools/endsession', {
		'auth' : db_auth,
		'form' : session_data
	}, function(err, res, body){
		// Do nothing on response
	});
}

/// Starts an admin session by enabling tools that are waiting for admin presence and posting to the DB to log the session
function AdminArrival(){
	// An admin has arrived, so clear the countdown that was initiated when the previous admin left
	clearTimeout(admin_left_timeout);

	admin.present = true;

	// Start tool sessions for each tool that is "off" due to lack of admin presence
	while(tools_queue_req_admin.length != 0) {
		StartToolSession(tools_queue_req_admin.pop());
	}

	// POST the session to the DB for logging
	request.post(db_ip + '/db/volunteers/startsession', {
		'auth' : db_auth,
		'form' : admin
	}, function(err, res, body) {
		// Do nothing on response
	});
}
/// Ends an admin session by disabling tools that are on and require admin presence (after a timeout) and posting to the DB to log the session
function AdminDeparture() {
	// POST the session to the DB for logging
	request.post(db_ip+'/db/volunteers/endsession', {
		'auth':db_auth,
		'form':admin
	}, function(err, res, body){
		// Do nothing on response
	});

	// Execute actions related to admin leaving (i.e. turning off dependant tools) after a set timeout
	// This allows another admin to arrive before tool functionality is disrupted
	admin_left_timeout = setTimeout(function() {
		console.log("Admin has left and time expired");

		admin.present = false;

		// End tool sessions for each tool that is "on" and requires admin presence
		while(tools_running_req_admin.length != 0) {
			EndToolSession(tools_running_req_admin.pop());
		}

		// Clear the admin info
		admin.name    = null;
		admin.uid     = null;
		admin.pitt_id = null;
		admin.room    = null;
	}, delay_time_after_admin_leaves_ms);

}

/// Utility function to remove a string from a list of strings
function RemoveToolByToolName(tool_array,tool_of_interest){
	index_to_remove = -1;
	tool_array.forEach(function(item, index) {
		if(item.toolname == tool_of_interest) {
			index_to_remove = index;
			return;
		}
	});
	if(index_to_remove != -1){
		tool_array.splice(index_to_remove, 1);
	}
}
