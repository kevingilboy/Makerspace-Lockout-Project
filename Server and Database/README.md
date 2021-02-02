# Server and Database
A NodeJS server and MySQL database were hosted off-premise that could be queried through a REST API. The database included information on users, visits, tools, and volunteers as described below.

The server included a site that allowed students to sign-in to the Makerspace, as well as admins to add/remove users from the database in a friendly way. I have this code and will upload it in the near future.

When the in-room Pi access point wanted to know if someone was authenticated for a tool, it would send a request to a REST endpoint of the server, which would respond to let the Pi know if the user was authenticated.

## Database Tables
`> use makerspace; DESCRIBE users;`
| Field              | Type            | Null    | Key | Default | Extra
| -------------------|---------------- | ------- | --- | ------- | -----
|num           |int(11)     |NO |  PRI |NULL    |auto_increment
|time          |datetime    |YES|      |NULL|
|name          |varchar(64) |NO |      |NULL|
|uid	          |varchar(8)  |YES|      |NULL|
|makerspace_id |varchar(8)  |YES|      |NULL|
|activities    |text        |YES|      |NULL|
|tools         |text        |YES|      |NULL|
|materials     |text        |YES|      |NULL|

`> use makerspace; DESCRIBE visits;`
| Field              | Type            | Null    | Key | Default | Extra
| -------------------|---------------- | ------- | --- | ------- | -----
|num           |int(11)     |NO |  PRI |NULL    |auto_increment
|time          |datetime    |YES|      |NULL|
|name          |varchar(64) |NO |      |NULL|
|uid           |varchar(8)  |YES|      |NULL|
|makerspace_id |varchar(8)  |YES|      |NULL|
|activities    |text        |YES|      |NULL|
|tools         |text        |YES|      |NULL|
|materials     |text        |YES|      |NULL|

`> use makerspace; DESCRIBE tools;`
| Field              | Type            | Null    | Key | Default | Extra
| -------------------|---------------- | ------- | --- | ------- | -----
|num	            | int(11)     |NO   |PRI  |NULL|   auto_increment
|name             |varchar(64) |NO|        NULL|
|pitt_id          |varchar(16) |YES|       NULL|
|uid              |varchar(8)  |YES|       NULL|
|admin_name       |varchar(64) |YES|       NULL|
|admin_pitt_id    |varchar(16) |YES|       NULL|
|admin_uid        |varchar(8)  |YES|       NULL|
|tool             |varchar(32) |NO |       NULL|
|time_activated   |datetime	 |YES |      NULL|
|time_deactivated |datetime    |YES|       NULL|

`> use makerspace; DESCRIBE volunteers;`
| Field              | Type            | Null    | Key | Default | Extra
| -------------------|---------------- | ------- | --- | ------- | -----
|name             |varchar(64) |NO |      |NULL|
|uid              |varchar(8)  |YES|      |NULL|
|pitt_id          |varchar(16) |NO |  PRI |NULL|
|affiliation      |varchar(16) |YES|      |NULL|
|major            |varchar(32) |YES|      |NULL|
|active_member    |tinyint(1)  |YES|      |NULL|
|active_volunteer |tinyint(1)  |YES|      |NULL|
|code             |tinyint(1)  |YES|      |NULL|
|training         |text        |YES|      |NULL|
