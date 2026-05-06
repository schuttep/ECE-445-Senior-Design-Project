# January 21, 2026 - First Group Meeting
**Objective**: We are meeting for the first time to try and nail down what we want to do for potential projects. We are going to come up with three ideas, one for each of us to post to the web board. We will hopefully have one that we like the most that we will follow

**What got done**: We came up with a couple different ideas. I came up with the idea to do a universal game board using an E-ink display. Danny came up with the idea to do a auto dealer poker machine. And Payton came up with the idea to do two chess boards that could play each other over the internet. We liked that one the best, but we all posted our ideas to get the course staffs thoughts. 

# January 24th, 2026 - Meeting To Get Ideas On Paper
**Objectives**: Meet to get final ideas of our project and put them on paper.

**What Got Done**: We fully decided on the Networked Physical Chess Board for Remote Play. We decided that we wanted to connect to the internet and have LEDs display the moves the other player made. We would also have a touch display UI. We put these ideas into an informal doc that we could refer to later

# January 28th, 2026 - Wrote RFA
**Objectives**: Write and submit our request for approval early for extra credit.

**What Got Done**: We officially got our idea down on paper and submitted to PACE. We outlined our problem, solution, subsystems, criterion for success, and component selection. 

* Hall effect sensor: [DRV5055A4QDBZR 12.5 mV/mT, ±169-mT Range](https://www.digikey.com/en/products/detail/texas-instruments/DRV5055A4QLPG/9371283?s=N4IgTCBcDa4JwDYC0AWOKNIHIBEQF0BfIA)
* MCU: [ESP-32 (includes Wi-Fi antenna and capability)](https://www.digikey.com/en/products/detail/espressif-systems/ESP32-S3-WROOM-1-N8R8/15295891)
* ADC: [TLA2528IRTER 12-bit, 8-channel, I2C](https://www.digikey.com/en/products/detail/texas-instruments/TLA2528IRTER/12328606?s=N4IgTCBcDaICoBkCCYCsYAcBJASnAojiALoC%2BQA)
* Display: [DSI Touch Screen LCD Display 800x480](https://www.dfrobot.com/product-3025.html)


# February 2nd, 2026 - Buying Breadboard Parts
**Objectives**: Buy our initial breadboard parts like hall effect sensors, magents, and ADCs.

**What Got Done**: We had to change the ADC we picked initially due to lack of stock on Digikey. We opted to change to the [ADS7128IRTER](https://www.digikey.com/en/products/detail/texas-instruments/ADS7128IRTER/12696006) which offers similar performance with less advanced features and a lower cost. We also bought [QFN-16 3x3, 0.5MM DIP ADAPTER](https://www.digikey.com/en/products/detail/schmalztech-llc/ST-QFN-16-3X3-05/24394924) to allows us to use the same package ADC in our breadboard. The [magnets](https://www.digikey.com/en/products/detail/radial-magnets-inc/8195/555329) we bought should be strong enough, but if they are not they give us a base reading of how sensitive the hall effects are to distance.

# February 10th, 2026 - Getting Ready for the First TA Meeting
**Objectives**: We need to have a more concrete diagrams for our first TA meeting of the semester. 

**What Got Done**: The most important part of this was getting a block diagram together that showed our different subsystems. We showed the hardware that each of our subsystems would need. 

# February 11th, 2026 - First TA Meeting
**Objectives**: Get feedback from our TA on our initial high level designs.

**What Got Done**: We got mostly posotive feedback from our TA. There was some confusion from the block diagram about how things would be physically layed out but this was cleared up with some discussion. Overall it seems like we are heading in the righ direction. 

# February 11th, 2026 - Digikey Order Disruption
**Objectives**: The Digikey order for breadboard parts hasn't had any updates and the order has an error looking it up.

**What Got Done**: I called Digikey support and they said there was an error and that I would need to reorder. They are reimbursing for the cost but can not expedite shipping. Fortunatly the parts will still come in time.

# February 13th, 2026 - Finished Writing The Project Proposal Document
**Objectives**: Need to submit our Project Proposal Document.

**What Got Done**: We finished writing this document getting into more detail on specific componenets, subsystems, and physical design. 

# February 14th, 2026 - Need Clarification on PCB size
**Objectives**: We need to understand the constraints of the PCB size we can order.

**What Got Done**: I emailed Professor Fliflit to clarify. He told us that we are limited to 10cm square. Our original idea was to create a single PCB the size of our chess board (12" square), but now we need to find a new strategy. We are now going to do a single board with MCU, power, and ADCs. The hall effect sensors will sit under each game square and wire directly to our headers for the ADC.

# February 15th, 2026 - Figuring Out the Display Connector
**Objectives**: We want to know how to connect our touch display to our ESP32.

**What Got Done**: We have this [touch dispaly](https://www.dfrobot.com/product-3025.html) from an old project Payton was working on. It orginally connects to a Raspberry Pi. However, it connects through a ribbon cable and we need to know if we can use that connector or a different one. We found that the ribbon cable was for a GDI port. This isn't natively supported on the ESP32 and would require a bridge chip or a converter. We decided aginast this because there are also connection points on the screen for SPI, I2C, and other analog control signals. This makes it so that our ESP32 can directly wire to the display through jumper wires. 

# February 16th, 2026 - Main PCB Schematics
**Objectives**: Finish up the main PCB schematic for routing later.

**What Got Done**: For this I knew we needed the ESP32 setup, power regulator, connectors, and ADCs.

For the ESP32 setup I used the [ECE 445 Wiki page](https://courses.grainger.illinois.edu/ece445/wiki/#/esp32_example/index) example board. This has all the proper boot/strapping pins, buttons, and programming circuit. Our final goal is to program and power through USB but to be safe I added this extra UART programming circuit. 

For the power regulator we need to step down 5V to 3.3V. I chose the [TLV62569](https://www.ti.com/lit/ds/symlink/tlv62569.pdf?HQS=dis-dk-null-digikeymode-dsf-pf-null-wwe&ts=1777992841599&ref_url=https%253A%252F%252Fwww.ti.com%252Fgeneral%252Fdocs%252Fsuppproductinfo.tsp%253FdistId%253D10%2526gotoUrl%253Dhttps%253A%252F%252Fwww.ti.com%252Flit%252Fgpn%252Ftlv62569) for its simplicity and efficiency. The only math needed was for the feedback voltage divider which follows $V_{out}=0.6V(1+\frac{R_1}{R_2})$. The data sheet recommends $R_2=100k\Omega$ for the best transient response time and current consumption. Solving the equation we get $R_1\approx450k\Omega$. This value had to be changed from the original solution so that it fit a standard value. The datasheet then outlines that for $1.8\leq V_{out}$ we need a $2.2\mu H$ indcutor and three output capacitors valued $10\mu F$. I shorted the enable pin with the $V_{in}$ pin so that the regulator is always on. I also added a solder jumper so that we can test the power system first as to not short other parts. For extra debugging I added an LED that will indicate when power is on.

For the [ADCs](https://www.ti.com/lit/ds/symlink/ads7128.pdf) I put them on their own hierarchical sheet for readability. All the ADCs are on an I2C bus and have an alert pin to the ESP32. We don't know if we will use the Alert pin yet or just poll the I2C bus but I added it so we have the option to use it. Each ADC also gets its own I2C address that is assigned through a resistor network between Decap and ADDR pins. I also added decoupling capacitors on the power output pins for the hall effect sensors. I did this because the wires to power the hall effect sensors will be quite long and thus will have a lot of inductance which these capacitors will negate. 

For connectors we chose to use standard pin headers to connect the display and hall effects. We then have a USB-C 2.0 port for 5V power and programming. I also added pin headers for power and I2C probing. 

# February 23rd, 2026 - Initial Breadboard Setup
**Objectives**: We want to set up our breadboard for the upcoming breadboard demo. This means setting up 8 hall effect sensors that connect ot an ADC which goes to our ESP32 dev board. We then want to also connect our LEDs.

**What Got Done**: We get the hardware set up on a large breadboard with hall effects set up 1.5" apart. We soldered the ADC to the breakout board and wired it to the ADC. We had some issues connecting to the ADC but were able to resolve it by reconfiguring the ESP and resoldering our ADC. We also found that our magnets were too weak and required almost touching the hall effect for more than a .1V difference in reading. We got our setup mostly done and Payton is going to work on writing code for the LEDs. 

# February 25th, 2026 - PCB Main Board Routing
**Objectives**: Route the Main PCB so we can submit before the first board order round. 

**What Got Done**: Finished routing the PCB. I needed to change some pins on the ESP32, but found a way to duplicate the 8 ADC layouts. I kept the power regulator as far from everything as I could to reduce noise on digital lines like I2C. 

# March 2nd, 2026 - Design Review
**Objectives**: Present our design document and get feedback from our peers and Prof Fliflit.

**What Got Done**: We got mostly positive feedback, but some things we needed to fix. Fliflit told us to add a section about tolerances and to adjust our schedule to be more precise.

# March 5th, 2026 - Breadboard Demo Planning & What Needs to Get Done Before Spring Break
**Objectives**: Plan our our breadboard demo and assign tasks for spring break.

**What Got Done**: We planned out how we would demo and ensured that all our components worked. We then assigned tasks like code that needed to be done and hardware validation. 

# March 5th, 2026 - Putting Together Purchase Order for First PCB assembly
**Objectives**: Get digikey order together for one Chess Board.

**What Got Done**: I used the BOM feature in KiCAD to find all the parts on Digikey. Everything was in stock and priced well so no issues. I also ordered a couple different sample magnets that we could test. The order is supposed to come after spring break. 

# March 9th, 2026 - Breadboard Demo
**Objectives**: Demonstrate piece detection and LED subsystem as well as show we can program the ESP.

**What Got Done**: We were able to deomonstrate all of the subsystems we want and Prof Fliflit was impressed. This is a good demo that, if transfers to our PCB correctly, we are close to having our hardware complete. 

# March 24th, 2026 - First PCB Assembly
**Objectives**: We got our parts in during spring break so we are now able to assemble our first PCB. The goal is to fully assemble one and validate it. 

**What Got Done**: We were able to solder everything on using the stencil and reflow oven for SMD parts. We then hand soldered the through holes. We did not have time to validate today but will come in tomorrow.

# March 25th, 2026 - Main PCB Testing
**Objectives**: Validate the Main PCB hardware.

**What Got Done**: We tested first for shorts and found the usb port we ordered was the wrong package having 6 pins instead of 12. This was shorting our 5V to GND. I removed this and then check again and found no shorts. I then checked each pin of the ADCs, I2C pins, and MCU pins. Everything looked so I powered the regulator and read a 3.29V output which is more than accurate enough. I then bridged the solder jumper and powered the board again. Same 3.29V reading so no other issues. This validates the basic electrical capabilites of our PCB

# March 27th, 2026 - Program Main PCB For The First Time
**Objectives**: We want to program the PCB with the breadboard demo code to show it translated to our PCB.

**What Got Done**: We were able to successfully flash the code after some slight issues. First, we were flashing while the board sat on our stencil which is metalic and caused shorting. Luckily nothing broke. We then tried reading the the ADC but had the wrong I2C pins in the program. We were able to validate our program worked.

We then had a discussion about the physical aspect of our board. Our biggest concern was mounting the hall effect sensors to the bottom of a wooden board. The ideal situation would be to mount them on a PCB or breadboard. To solve this issue we decided we could make each game tile its own PCB that has a sensor mounted on it. We priced this out and for 150 (75 black and 75 white) 1.5" square pcbs it would only be $13 with shipping being more expensive at $45. We decided that this would be our best path forward as we could also 3d print the rest of the enclosure. 

# March 30th, 2026 - Tile PCB Schematic and Routing
**Objectives**: Create and purchase the tile PCBs.

**What Got Done**: I created the schematic for the Tile PCBs which is quite simple. It is only the hall effect sensor, a decoupling capacitor, and pin headers for power/gnd daisy chaining, LEDs, and analog sensor output. We already ordered the through hole hall effect sensors but to mount them in the correct orientation and centered, we can solder them to the top of the through hole pads. We then ordered the PCBs and pin headers for them. 

# April 12th, 2026 - 3D Modeling Tile Supports
**Objectives**: Model the supports for the Tile PCBs

**What Got Done**: I found a [dove tail connected chess board](https://www.thingiverse.com/thing:1269307) online that I modified to have four supports with M2x6mm screw holes. I also had to create 2 other varients with. One with both dove tails cut off for the corner, and one with only one cut off for the edges. The issue we are running into with these is the quantity needed and print time. We need a total of 128 and 25 take 7.5 hours. This means we need over 37.5 hours of print time just for these. The other issue we ran into is that the dove tailes are not tight enough and slide around easily. Our solution to this is to create a walled baseplate that the tile supports will sit in. We chose not to createa a baseplate with supports so that if one tile breaks it is easier to service and repair. We are starting printing.

# April 15th, 2026 - 3D Modeling Baseplates & Getting Screws
**Objectives**: Create the baseplates for the tile supports.

**What Got Done**: For the size of the game board, we need to have 4 prints for the baseplates. We need 2 for the corners in the back, 2 with just one side with walls, and then supports for both the screen and main PCB. We also found [game pieces](https://www.printables.com/model/1552876-simple-set-of-chess-pieces/files) that we modified to have press fit holes for the magnets. When all of these are done we will glue the baseplates together. 

I also went out to find screws. I needed 512 M2x6mm and 8 M3x3mm screws. I went to a local hardware store and they did not have the quantities we needed. So, we went to McMaster-Carr and the screws will come tomorrow. These were failry cheap getting 600 M2 and 50 M3 for $40.

# April 16th, 2026 - Assembly of the Physical Board
**Objectives**: Assemble and validate the first board.

**What Got Done**: To do this we needed to screw all the tile PCBs to the supports and then wire each column to the main PCB. We have been slowly soldering the tile PCBs together over the past week. It took a couple hours to get everything assembled. We had one issue with getting the LEDs to fit because of the wiring of power and sensors. We decided that we could discard the LED subsystem in favor for a UI on the display. This was unfortunate but not a detriment to our high level requirments. 

We then validated the board finding that one of our ADCs was not working. I tried soldering it again, checking for shorts, and replacing it. Nothing works but after using a new one and reworking it a bit we were able to read from it. At ths stage our physical board is done and now Payton and Danny will start implementing their code. 

# April 23rd, 2026 - Mock Demo
**Objectives**: Get feedback on our demonstration and show that one entire board works.

**What Got Done**: We had an issue where we couldn't connect to the illinoisnet wifi. However, Payton was able to connect to his home wifi. We found that the issue was that the board we were using was the second board we assembled. We had never registered this board's MAC address with the university so it could not connect to the wifi. The demo went ok besides that and our TA said if we can get a second board done we are in a great position. After fixing the wifi issue, we were able to connect to our server and play a game between the board and server. 

# April 24th, 2026 - Mock Presentation
**Objectives**: Get feedback on a shortened version of our presentation

**What Got Done**: We got good feedback about using less words and more images. As well as changing our block diagram to be more readable. 

# April 27th, 2026 - Second Board Done
**Objectives**: Get the second board done and validate. 

**What Got Done**: This assembly went easier because we had shorter wires to work with for daisy chaining. Once the board was assembled we flashed it and it worked. No issues with this assembly. 

# April 28th, 2026 - Prep For Final Demo
**Objectives**: Get ready for our final demonstration. 

**What Got Done**: We scripted out a game that will show edge cases like castle, en passant, check mate, and capture. We also want to show off our new AI features like hints and versus AI. We are prepared for demo. 

# April 29th, 2026 - Final Demo
**Objectives**: Present our final demonstration with two working boards. 

**What Got Done**: We were able to demonstrate two working boards playing on seperate networks and worked through our scripted game. We answered all questions asked. 

# April 30th, 2026 - Prep For Final Presentation
**Objectives**: Go over last minute ideas and get ready for presentation.

**What Got Done**: We cut out parts we thought were time wasters and reformated to UIUC ECE slides. We feel prepared and that we will be timely in our presentation.

# May 1st, 2026 - Final Presentation & Demonstration Video
**Objectives**: Present our project.

**What Got Done**: Presented and defended our project and its design. Got added as an honorable mention project.