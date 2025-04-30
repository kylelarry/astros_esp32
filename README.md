# astros_esp32
My apologies for the barebones code, documentation and repo, I'm still learning how to porperly use Git and C.


Older version of the display in action: https://packaged-media.redd.it/eda1pjg27vxe1/pb/m2-res_1320p.mp4?m=DASHPlaylist.mpd&v=1&e=1746032400&s=cbfca95a92140ceb9b541c23411f3144432116af#t=1.952874

This was built on a non-standard esp-32 based ["Cheap Yellow Display]"(https://randomnerdtutorials.com/cheap-yellow-display-esp32-2432s028r/) aka CYD

How to build one for yourself.

Step 1: Choosing your device
  I used this one from [aliexpress](https://www.aliexpress.us/item/3256807989694645.html?spm=a2g0o.order_list.order_list_main.5.1b8e1802ZdUjR9&gatewayAdapt=glo2usa). (It was $8 when I bought it but i guess they raised the price recently)
    Pros of using this specific one
      Easiest compatibility with this specific project
    Cons of this specific board
      Less simple compatibility with other ["Cheap Yellow Display"](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display?tab=readme-ov-file) projects
      I have to hold the boot button to successfully upload code to the board. [Supposedly this would fix that issue, but I haven't tried this myself.](https://randomnerdtutorials.com/solved-failed-to-connect-to-esp32-timed-out-waiting-for-packet-header/) I'd assume the more standard CYD boards don't have this issue but I am not sure.

Step 2: Initial setup (Once you have your board)
  Download and install [Arduino IDE](https://www.arduino.cc/en/software)
  Install the required libraries (Tools>Manage Libraries:
    #include <Arduino_GFX_Library.h> (GFX Library for Arduino by Moon on our Nation)
    #include <WiFi.h>
    #include <HTTPClient.h>
    #include <ArduinoJson.h>
    #include <XPT2046_Touchscreen.h>
    #include <SPI.h>
  Plug the ESP32 into your pc
  
  Select the ESP32 (I've been inputting it as ESP32 Dev Module) ![image](https://github.com/user-attachments/assets/5bb87563-848b-4036-9c13-09bac115f5f7)

Step 3: Adding the Code
  Paste my strostouch file into a new project
  Edit the wifi credentials to match the network you want to connect it to.
  ![image](https://github.com/user-attachments/assets/fc35ed5b-5846-4840-937e-4986d56962a0)

  Hit the arrow to send your code to the device
  
  ![Hit the arrow to upload your code to the device](https://github.com/user-attachments/assets/d93e5442-1a8b-4c54-aa4e-e14def224ebd)

  This will bring a lot of output up in a terminal, it will take a little time (particularly the first time you upload the code)
  Once you see the "Linking everything together" (or before that if you want to be safe) Hold down the boot button on the back of the board. While holding the boot button, tap the reset button once and release it. 
  If you're getting an error, try doing the hold button technique before uploading the code
  
  ![image](https://github.com/user-attachments/assets/218bff3a-e8ca-49a4-a5cc-152092cba571)

Keep holding the boot button until a second after you see the "Writing at ___" messages begin, then release the button.

![image](https://github.com/user-attachments/assets/62ec6d04-305c-485a-b0d3-219eb18ad549)

Congrats the board should be working now!

# Current Issues with this code:
  ñ and other special characters dont display properly
  The batter section sometimes has an extra page
  Before 9am the board will show yesterdays info
  The game time is done in a pretty lazy way (the api gives it to me in UTC)

# Possible future features:
  find an excuse to add the home run bull and cowboy somewhere
  Season Stats section
  Future Games Calendar
  
  
  


