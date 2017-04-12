# CREATORS:

Koray Gocmen
Jinghan Guan


## HOW TO USE?
---

#### SERVER:

1. Start the server "./server 4430"

#### CLIENT:

1. Start the client "./client"
2. Login with a user "/login user_a 12345 127.0.0.1 4430"
3. Create a session "/createsession test"
4. Join the session "/joinsession test"
5. Another user joins "/login user_b 67890 127.0.0.1 4430"
6. Invite user to your session "/invite b"
7. Messaging to the session "this is a message"
8. Leave session "/leavesession"
9. Exit the user "/exit"


#### NOTES
---
* A user can only join to one session
* You can only invite other users to your session
* Server can only handle 10 online users at the same time
