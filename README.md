# Simple and cheap one hand Hall Effect (aka Magnetic) Switch Keyboard
Based on Arduino Leonardo, fully coded by myself.<br>
## Now achieved:<br>
1. RT function, adjust parameter `rt_active` and `rt_deactive` to change sensitivity.<br>
2. Deadzone function, adjust parameter `high_deadzone` and `low_deadzone` to change deadzone.<br>
3. EEPROM memory function, all parameter write and read with EEPROM,
thus, do not change variable in main code use other eeprom write code to change it for now.<br>
(the calculation of the parameter is `(Top-Bottom)*($para/1000)`, or you can say "divide the full switch travel into 1000 units",
 although the Hall Effect unit might not be so sensitive enough to maximum the precision.)<br>
5. simple EC11 detect, you can adjust volume and bind key on it.
## To do list
1.Key binding software<br>
2.3D-print shell<br>
3.stm32 version<br>
