2025-10-01
Code is running. For some reason it restarted because of brownout.
Monitoring only works just after flash. Maybe we need to set speed.

We need a bulk capacitor to prevent brownout

When searching for parts, look for:

"Electrolytic capacitor" - this is the capacitor type
Values: 470µF, 1000µF, or anywhere in between
Voltage rating: at least 6.3V (10V or 16V is common and fine)
Through-hole or SMD depending on your board


2025-07-21
When programming, GPIO8 needs to be high. GPIO9 needs to be low.
As we do not use GPIO8 for anything else we should be able to just set it to high.

Moved button pin from 7 to 4. It now works
TX and RX to 0 and 1. 

Programming works. Remember to disable logs.

Dual LED diode behaving strangely.

2025-06-10  
Found out my power circuit is wrong. I had the supercapacitor connected in series and not in parallel to the voltage regulator.
Also, the voltage regulator I'm using needs 4.5V because of a slight voltage drop. I need to use an MCP1700-3302E.

Use 10kΩ + 10kΩ voltage divider to read VCC voltage