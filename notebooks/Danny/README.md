# Danny's Worklog

# 2026-01-21 - First Group Meeting
Met with the team for the first time to discuss possible project ideas. We each came up with an idea to post to the web board. Payton suggested two physical chess boards that could play each other over the internet. We liked the chess board idea the most because it felt the most realistic and interesting, but we still posted all three ideas to get feedback from course staff.

# 2026-01-24 - Getting Ideas on Paper
Met with the team again to narrow down the project idea and start putting more details together. We officially decided to move forward with the Networked Physical Chess Board for Remote Play. The main idea was to have two physical boards connected over the internet so two players could play remotely. We also decided that LEDs would show the opponent’s moves and that the board would include a touch display UI. We wrote these ideas into an informal document so we could keep track of the early design direction.

# 2026-01-28 - Request for Approval
Worked with the team to write and submit our Request for Approval early for extra credit. We wrote out the problem we wanted to solve, our proposed solution, the main subsystems, our criterion for success, and the major components we planned to use. At this point we were planning on using Hall effect sensors, an ESP32, an I2C ADC, and a touch screen display.

# 2026-02-02 - Buying Breadboard Parts
Worked on getting our initial breadboard parts ordered. This included Hall effect sensors, magnets, ADCs, and adapters so we could actually test the parts on a breadboard. We had to change our original ADC because it was not in stock, so we switched to the ADS7128. This still gave us the functionality we needed while also being cheaper and easier to get. We also ordered magnets so we could test how sensitive the Hall effect sensors were at different distances.

# 2026-02-10 - Preparing for First TA Meeting
Met with the team to prepare for our first TA meeting. The main focus was getting a better block diagram together so we could explain our project at a high level. We broke the design into the major subsystems and listed the hardware each part would need. This helped us explain how the sensing, display, WiFi, and game logic would all connect.

# 2026-02-11 - First TA Meeting
Met with our TA to go over the initial high-level design. We got mostly positive feedback. There was some confusion at first about how the physical layout of the boards would work, but we talked through it and cleared it up. Overall it seemed like we were moving in the right direction.

# 2026-02-11 - DigiKey Order Issue
There was an issue with our DigiKey order for the breadboard parts. The order had not updated and there was an error when trying to look it up. We contacted DigiKey support and found out that the order had to be placed again. They reimbursed the cost, but they could not speed up shipping. Luckily the parts were still expected to arrive in time for us to use them.

# 2026-02-13 - Project Proposal Document
Finished writing the Project Proposal Document with the team. This document went into more detail than the RFA and included our subsystems, component choices, physical design, and overall plan for the project. It helped turn the early idea into a more concrete design.

# 2026-02-14 - PCB Size Clarification
We needed to figure out the actual PCB size limits before finalizing our hardware design. We found out from Professor Fliflet that we were limited to a 10 cm square PCB. Our original plan was to make one large PCB the size of the chess board, but that was not allowed. We adjusted the plan so the main PCB would hold the MCU, power circuitry, and ADCs. The Hall effect sensors would sit under the individual chess squares and wire back to headers on the main board.

# 2026-02-15 - Display Connector Planning
Worked on figuring out how the display would connect to the rest of the system. Since the display was an important part of the user interface, we needed to make sure the connector and wiring made sense for the ESP32 and our physical board layout. This was mostly planning and checking compatibility so we would not design around a display connection that would be difficult to use later.

# 2026-02-16 - Main PCB Schematics
Worked on the main PCB schematic for the project. The goal was to get the ESP32, ADCs, power, headers, and supporting components connected correctly before routing the board. This was an important step because the main board would become the center of the whole system and connect to the Hall effect sensors under the chess tiles.

# 2026-02-23 - Initial Breadboard Setup
Met with the team for the first time to discuss possible project ideas. We each came up with an idea to post to the web board. I suggested an auto dealer poker machine, another idea was a universal game board with an E-ink display, and Payton suggested two physical chess boards that could play each other over the internet. We liked the chess board idea the most because it felt the most realistic and interesting, but we still posted all three ideas to get feedback from course staff.

# 2026-03-02 - Discussion with Professor Fliflet
Discussed project at Design Review with Professor Fliflet. He suggested we add tolerance section to our design document as well as splitting up the workload in the schedule section between all of our group members. 

# 2026-03-05 - Team Meeting
Met to discuss what needs to be completed by Sunday 03/08 and what needs to be done by the time Spring Break starts. Payton and I tasked with getting the breadboard code done before the Breadboard Demo scheduled for 03/09. Bought wooden enclosre to be used for testing components as we implement them. 

# 2026-03-09 - Magnet Demo
I met with the team to prepare and demo the magentic capabiltiies of our project. This included an array of 9 Hall Effect sensors which I wired to the breadboard to show the proof of concept for our magnetic capabilities. What this entailed was a full connection with the ESP32 and a flashed set of code that triggered LEDs when a magent passed over one of the sensors. Each LED was individually addressable, so hovering a magnet over one of the sensors only turned on its respective LED. This was a very successful demo and really showed what we could do.

# 2026-03-18 - Move Checking API Code
Our boards must check when a move is made whether that move is legal and whether it ends the game. For example, once a move is made maybe the player moved the piece in an illegal way (like moving a knight in a straight line). This is detectable because the checker first makes sure only 2 tiles were changed (source/destination tiles) or 4 tiles (in the case of castling). Once it knows this, it knows the previous board state and new state which means it knows which piece was moved and how it was moved. 

If a move is made and is deemed illegal, the move will not be able to be sent to the opponent and the display will show the move was not valid. This will give the player a chance to move the piece back and move again to make a legal move. 

If the player's move ends the game (checkmate/stalemate/time out) then the game will end for both players and the display will show who the winner/loser is. 

The move checking API relies heavily on one trick; the game board begins in the standard board state at the start of every game. This gives us information on every piece's location from the start. Once a move is made we can only detect one tile losing a piece and another gaining one. We do not know which piece was moved directly, but we do know the previous board (the first being the standard start state) so we can determine exactly which piece was moved and where it moved to. 

Once a legal move is made, if it is a legal move and doesn't end the game then the board state will be condensed into a FEN string (a representation of chess states using just one string) and will be sent over WiFi to the opposing board for it to read. LEDs will light up to display where the player moved which piece and the opponent can make the player's move on their board before being allowed to start making their move. 

The code for this move checking was written such that it can be eventually flashed to the ESP32 using the Arduino IDE. More debugging of this code is necessary and we also will figure out how to connect this string-checking code to the actual physical boards. 

# 2026-03-24 - Main PCB Soldering
The main PCB for our project arrived. I went to the lab to check our components were all present. Once I confirmed everything was there, I began applying solder paste using the PCB stencil. One of the USB connectors we bought isn't exactly what we needed so we may have to order new ones. After applying the paste, placed all the components on the board in the pasted slots. Once that was done, started up the reflow oven following the directions on the wall. Placed PCB in the oven and waited as it coldered our components to the board. Once the board was taken out a shorts check was done and we luckily had no bridging/shorting. Next stpes are to potentially program the board, build the physical chess board itself, and begin writing the backend chess game logic. 

# 2026-04-10 - Tile PCB Soldering
In our project, the tiles of the chess board are going to be individual black/white PCBs with a Hall Effect sensor underneath each one. These tile PCBs arrived and we began soldering them. Each tile has a few components to add. My task was to solder soem of these components to these boards. 

# 2026-04-21 - Completed Single Board
We fully assembled our first board. All the tiles were put in place and the wiring was completed. Next steps will be to work on the software to get a working game between the assembled board and our demo website where we can send and receive moves as an "opponent". 

# 2026-04-22 - Debugging Display + FEN Reading
Spent time debugging issues with the display and how FEN strings were being read and interpreted on the board. The display was not updating correctly after moves, so traced through the code to see where the breakdown was happening between receiving the FEN string and rendering the board state. Adjusted parts of the logic to correctly read FENs from the ADCs. Also worked on making sure the display properly reflects invalid moves and resets correctly when needed. Screen was also rotated incorrectly, so fixed it to face the player. 

# 2026-04-23 - Mock Demo + Working Single Board Game
Held our mock demo with our TA to simulate how the full system will function during presentation. During the demo there was a slight hiccup that prevented us from fully being able to show the working single board. After the demo we worked more and realized we were reading an incorrect MAC address for the board to be able to properly interface with the university WiFi. At this point we successfully got a single board fully working with move detection, validation, and display updates. The board can now detect piece movement, verify legality using our move checking logic, and update the display accordingly. It also correctly displays checkmates as well. This was a big milestone because it shows that the core gameplay loop works on one board. Focus moving forward will be first on printing, soldering, and assembling a second board. Then we will shift focus to syncing two boards together over WiFi and polishing the overall user experience on the displays for the final demo.

# 2026-04-27 - Final Demo Prep
Met with team to discuss what order we will display our functionality in. Also developed a move sequence to show implementations of En Passant, Promotions, Castling, and Checkmate. This way we have a predetermined game we will run through with for the professor to see our full functionality. The move order is as follows:

1. e4 f6
2. e5 d5
3. exd6 e.p. Bd7 (En Passant)
4. dxc7 g5 (Capture)
5. cxb8=Q Rxb8 (Promotion)
6. Nc3 a6
7. d3 a5
8. Bd2 a4
9. Qg4 a3
10. O-O-O b6 (Castling)
11. Qh5# (Checkmate)

We will talk throughout while also playing the game against the two boards. We decided to split the talking points between us to highlight what each of us specialized in this project. 

# 2026-04-28 - Final Presentation Prep
Worked on my portion of the Final Presentation slides today. Included all the information on FEN strings and how move validation works in the backend. Rehearsed a bit alone and then discussed with the team what order we will go in for the final presenting. 

# 2026-04-29 - Final Demo
Demo'd our project to the professor. We accurately showed all the features we intended to, including using the game order above, showing WiFi connections, chat/hint features, and reconnecting if connection goes out. Next step is to finalzie how we will do our Final Presentation. 

# 2026-05-01 - Final Presentation
Presented our Final Presentation to the professor, TA, and peer reviewers. Answered some questions about connectivitiy, physical design process/issues, and how our backend processes moves and validates board states. 

# 2026-05-02 - Final Video
Discussed what should be included and in what order for the final video. Teammates then recorded themselves playing games against each other at their apartments and a voice recording was made explaining the game and moves as they were being made. Submitted this video.

# 2026-05-04 - Awards Ceremony
Selected as an Honorable Mention! Attended awards ceremony in ECEB and picked up our certificate. 