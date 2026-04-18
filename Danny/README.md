# Danny's Worklog

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
