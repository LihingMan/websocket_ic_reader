# websocket_ic_reader
- Using any Windows compliant smart card reader, this program is able to connect to the reader and grab Malaysian IC data from it. 
- With proper registry keys enabled, this program can then transmit the grabbed data via a websocket to an open browser client for analysis/display.

## Setting the registry keys
1. Go to Start then in Find type **regedit** -> it should open "Registry Editor"

2. Right click **HKEY_CLASSES_ROOT** and then click on **"New" -> "Key"**

3. Name the created "Key" with the custom URI name you want (e.g. icreader)

4. Right click the created **"Key" (icreader) -> then "New" -> "String Value"** and add URL Protocol without any value

5. Add more entries like in Step 2. (Right click -> **New** -> **Key**). Create a hierarchy like: **icreader -> shell -> open -> command**

6. Inside "command" change the **"Default"** value into the path to the ".exe".  (Double click **"Default"** to open up the editor)

7. Wrap the path in "" as shown in the image and then follow it up with **"%1"** at the end. 
(e.g. "C:\Users\Dell\Desktop\ic_script\websocket_client.exe" "%1")

8. **"%1"** is for the .exe to accept arguments. 

9. To call the ".exe" from browser, call it via the key name. 
(e.g. icreader://<argument>)
