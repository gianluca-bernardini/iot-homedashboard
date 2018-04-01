/************************************************************************
* Copyright 2016 IBM Corp. All Rights Reserved.
*
* Watson Maker Kits
*
* This project is licensed under the Apache License 2.0, see LICENSE.*
*
************************************************************************
*
* Build a talking robot with Watson.
* This module uses Watson Speech to Text, Watson Conversation, and Watson Text to Speech.
* To run: node conversation.js

* Follow the instructions in http://www.instructables.com/id/Build-a-Talking-Robot-With-Watson-and-Raspberry-Pi/ to
* get the system ready to run this code.
*/

var watson = require('watson-developer-cloud'); //to connect to Watson developer cloud
var config = require("./config.js") // to get our credentials and the attention word from the config.js files
var exec = require('child_process').exec;
var fs = require('fs');
var Client = require( 'ibmiotf' );
var statistics = require('math-statistics');
var appClientConfig = require( './application.json' );
var conversation_response = "";
var attentionWord = config.attentionWord.split(','); //you can change the attention word in the config file
var talking = false;


/***********************************************************************
 * Step #0: Configuring the iot platform
 ***********************************************************************
 */

var appClient = new Client.IotfApplication(appClientConfig);
appClient.connect();

var homeSensorsStatus;
var deviceType = "ESP8266";
var deviceID = "ESP8266-e12-1";
var commandType = "update";

appClient.on("connect", function () {
    //subscribe to all events published by a device in json format
    appClient.subscribeToDeviceEvents(deviceType, deviceID, "+","json");
    // subscribe to the device status:
    appClient.subscribeToDeviceStatus(deviceType, deviceID);

});
appClient.on("deviceEvent", function (deviceType, deviceId, eventType, format, payload) {
    //console.log("Device Event from :: "+deviceType+" : "+deviceId+" of event "+eventType+" with payload : "+payload);
	var jsonPayload = JSON.parse(payload);
    homeSensorsStatus=jsonPayload.d;
});

/*
 *  Payload like the following:
 *  {"d":{"myName":"ESP8266-e12-1","humidity":37.00,"temp":21.00,"heat":20.12,"rain":5,"switch":0}}
 */
appClient.on("deviceStatus", function (deviceType, deviceId, payload, topic) {
    //console.log("Device status from :: "+deviceType+" : "+deviceId+" with payload : "+payload);
	var jsonPayload = JSON.parse(payload);
    homeSensorsStatus=jsonPayload.d;
});

appClient.on("error", function (err) {
    console.log("Error : "+err);
});

/************************************************************************
* Step #1: Configuring your Bluemix Credentials
************************************************************************
In this step we will be configuring the Bluemix Credentials for Speech to Text, Watson Conversation
and Text to Speech services.
*/

var speech_to_text = watson.speech_to_text({
  username: config.STTUsername,
  password: config.STTPassword,
  version: 'v1'
});

var conversation = watson.conversation({
  username: config.ConUsername,
  password: config.ConPassword,
  version: 'v1',
  version_date: '2016-07-11'
});

var text_to_speech = watson.text_to_speech({
  username: config.TTSUsername,
  password: config.TTSPassword,
  version: 'v1'
});

/************************************************************************
* Step #2: Configuring the Microphone
************************************************************************
In this step, we configure your microphone to collect the audio samples as you talk.
See https://www.npmjs.com/package/mic for more information on
microphone input events e.g on error, startcomplete, pause, stopcomplete etc.
*/

// Initiate Microphone Instance to Get audio samples
var mic = require('mic');
var micInstance = mic({ 'rate': '44100', 'channels': '2', 'debug': false, 'exitOnSilence': 6 });
var micInputStream = micInstance.getAudioStream();

micInputStream.on('data', function(data) {
  //console.log("Recieved Input Stream: " + data.length);
});

micInputStream.on('error', function(err) {
  console.log("Error in Input Stream: " + err);
});

micInputStream.on('silence', function() {
  // detect silence.
  console.log("Got SIGNAL silence");
});

micInstance.start();

console.log("T J Bot is listening, you may speak now.");

var textStream ;

/************************************************************************
* Step #3: Converting your Speech Commands to Text
************************************************************************
In this step, the audio sample is sent (piped) to "Watson Speech to Text" to transcribe.
The service converts the audio to text and saves the returned text in "textStream"
*/
console.log("attention words: " + attentionWord);

textStream = micInputStream.pipe(speech_to_text.createRecognizeStream({
  content_type: 'audio/l16; rate=44100; channels=2',
  interim_results: true,
  keywords: attentionWord,
  smart_formatting: true,
  keywords_threshold: 0.5
}));

textStream.setEncoding('utf8');

/*********************************************************************
* Step #4: Parsing the Text and create a response
*********************************************************************
In this step, we parse the text to look for attention word and send that sentence
to watson conversation to get appropriate response. You can change it to something else if needed.
Once the attention word is detected,the text is sent to Watson conversation for processing. The response is generated by Watson Conversation and is sent back to the module.
*/
var context = {} ; // Save information on conversation context/stage for continous conversation
textStream.on('data', function(str) {
  console.log(' ===== Speech to Text ===== : ' + str); // print the text once received
  var matchedWord = null;
  for (var i = 0; i < attentionWord.length; i++) {
	if (str.toLowerCase().indexOf(attentionWord[i].toLowerCase()) >= 0) {
		console.log("matched attention word " + attentionWord[i]);
		matchedWord = attentionWord[i];
		break;
	}
  }
  if (matchedWord!=null) 
  {
    var res = str.toLowerCase().replace(matchedWord.toLowerCase(), "");
    console.log("msg sent to conversation:" ,res);
    matchedWord = null;
    // get the context to pass to the conversation dialog:
    //var cachedEvents = appClient.getLastEventsByEventType(deviceType, deviceID, "status");
    //console.log(cachedEvents);
    var rainMessage = "no, it's not raining";
    if (homeSensorsStatus !== undefined) {
        context.humidity = homeSensorsStatus.humidity;
        context.temp = homeSensorsStatus.temp;
        context.heat = homeSensorsStatus.heat;
        if (homeSensorsStatus.rain > 50) {
            rainMessage = "yes, it is raining";
        }
        context.rain = rainMessage;
    }
    
    conversation.message({
      workspace_id: config.ConWorkspace,
      input: {'text': res},
      context: context
    },  function(err, response) {
      if (err) {
        console.log('error:', err);
      } else {
    	 // to process actions on the actuators, we need to handle the following intents/entities:
	    //intents: [ { intent: 'turn_on', confidence: 1 } ],
	    //entities: [ { entity: 'actuators', location: [Object], value: 'light' } ],
	    if (response.intents){
	    	for (var i in response.intents){
	    		var intent = response.intents[i];
	    		if(response.entities){
	    			for(var j in response.entities){
	    				var entity = response.entities[j];
	    				if (entity.entity === "actuators" && entity.value === "light")
	    				{
	    					var value = -1;
	    					if (intent["intent"] === "turn_on") {
	    						value = 1;
	    					} else if (intent["intent"] === "turn_off") {
	    						value = 0;
	    					}
	    					if (value >=0) {
	    						var myData = JSON.stringify({"actuator":entity.value,"val":value});
		    					//console.log(myData);	
		    				    appClient.publishDeviceCommand(deviceType, deviceID, commandType, "json", myData);	
	    					}
	    					
	    				} else if (entity.entity === "actuators" && (entity.value === "door" || entity.value === "window")) 
	    				{
	    					var value = -1;
	    					if (intent["intent"] === "open"){
	    						value = 1;
	    					} else if (intent["intent"] === "close"){
	    						value = 0;
	    					}
	    					if (value >=0) {
	    						var myData = JSON.stringify({"actuator":entity.value,"val":value}); 
		    					//console.log(myData);	
		    				    appClient.publishDeviceCommand(deviceType, deviceID, commandType, "json", myData);
	    					}
	    				}
	    			} 
	    		}
	    	}
	    }
        context = response.context ; //update conversation context
        conversation_response =  response.output.text[0]  ;
        if (conversation_response != undefined ){
    	  /*********************************************************************
    	  Step #5: Speak out the response
    	  *********************************************************************
    	  In this step, we text is sent out to Watsons Text to Speech service and result is piped to wave file.
    	  Wave files are then played using alsa (native audio) tool.
    	  */
          if (!talking)
        	  convertToSpeech(conversation_response);
        }else {
          console.log("The response (output) text from your conversation is empty. Please check your conversation flow \n" + JSON.stringify( response))
        }

      }

    })
  } else {
    console.log("Waiting to hear", attentionWord);
    
  }
});

textStream.on('error', function(err) {
  console.log(' === Watson Speech to Text : An Error has occurred =====') ; // handle errors
  console.log(err) ;
  console.log("Press <ctrl>+C to exit.") ;
});


function convertToSpeech(text){
  var modifiedText = "<voice-transformation type=\"Custom\" pitch=\"" + config.pitch + 
                     "\" pitch_range=\"" + config.pitch_range + 
                     "\" rate=\"" + config.rate + 
                     "\" glottal_tension=\"" + config.glottal_tension + 
                     "\" breathiness=\"" + config.breathiness +
                     "\" timbre_extent=\"" + config.timbre_extent +
                     "\" timbre=\"" + config.timbre + "\" >" + text + "</voice-transformation>"

  var params = {
    text: modifiedText,
    voice: config.voice,
    accept: 'audio/wav'
  };
 
  console.log("Result from conversation:" ,conversation_response);

  tempStream = text_to_speech.synthesize(params).pipe(fs.createWriteStream('output.wav')).on('close', function() {
	  talking = true;
	  var create_audio = exec('aplay output.wav', function (error, stdout, stderr) {
      if (error !== null) {
        console.log('exec error: ' + error);
      }
    });
    talking = false;
  });
}
