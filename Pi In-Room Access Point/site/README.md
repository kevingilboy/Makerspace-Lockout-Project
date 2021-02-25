# Site
This is the Makerspace server app for communicating with ESP8266-connected RFID readers and tool power sources over MQTT. It includes support for an admin-based buddy system and sending emails when a tool times out.

Fields in `app.js` like the SSID, wifi password, database username, database password, email address, and email password were omitted and labelled with `// TODO` for clarity.

To run, first ensure that npm, nodejs, and nodemon are installed. Then use `npm install` to install the dependencies of the app. Once completed, the app can be started by `npm start`.
