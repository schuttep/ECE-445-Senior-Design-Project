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
**Objectives**:

**What Got Done**:

# February 16th, 2026 - Main PCB Schematics
**Objectives**:

**What Got Done**:

# February 23rd, 2026 - Initial Breadboard Setup
**Objectives**:

**What Got Done**:

# February 25th, 2026 - PCB Main Board Routing
**Objectives**:

**What Got Done**:

# March 2nd, 2026 - Design Review
**Objectives**:

**What Got Done**:

# March 5th, 2026 - Breadboard Demo Planning & What Needs to Get Done Before Spring Break
**Objectives**:

**What Got Done**:

# March 5th, 2026 - Putting Together Purchase Order for First PCB assembly
**Objectives**:

**What Got Done**:

# March 9th, 2026 - Breadboard Demo
**Objectives**:

**What Got Done**:

# March 24th, 2026 - First PCB Assembly
**Objectives**:

**What Got Done**:

# March 25th, 2026 - Main PCB Testing
**Objectives**:

**What Got Done**: found usb was wrong package

# March 30th, 2026 - Tile PCB Schematic and Routing
**Objectives**:

**What Got Done**: 

# April 12th, 2026 - 3D Modeling Tile Supports
**Objectives**:

**What Got Done**: 

# April 16th, 2026 - 3D Modeling Baseplates & Getting Screws
**Objectives**:

**What Got Done**: 

# April 16th, 2026 - Assembly of the Physical Board
**Objectives**:

**What Got Done**: 

# April 16th, 2026 - First Board Done
**Objectives**:

**What Got Done**: 

# April 23rd, 2026 - Mock Demo
**Objectives**:

**What Got Done**:

# April 24th, 2026 - Mock Presentation
**Objectives**:

**What Got Done**: 

# April 27th, 2026 - Second Board Done
**Objectives**:

**What Got Done**: 

# April 29th, 2026 - Prep For Final Demo
**Objectives**:

**What Got Done**: 

# April 29th, 2026 - Final Demo
**Objectives**:

**What Got Done**: 

# April 30th, 2026 - Prep For Final Presentation
**Objectives**:

**What Got Done**: 

# May 1st, 2026 - Final Presentation & Demonstration Video
**Objectives**:

**What Got Done**: 