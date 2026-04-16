#include <Arduino.h>

// ----------------------------
// Basic piece helpers
// ----------------------------

// Returns true if p is a white piece
bool isWhitePiece(char p)
{
    return p >= 'A' && p <= 'Z';
}

// Returns true if the square contains a piece
bool isPiece(char p)
{
    return p != '.';
}

// Returns true if both pieces are the same color
bool sameColor(char a, char b)
{
    if (!isPiece(a) || !isPiece(b))
        return false;
    return isWhitePiece(a) == isWhitePiece(b);
}

// Returns true if p is a valid promotion piece
bool isValidPromotionPiece(char p)
{
    char t = tolower(p);
    return t == 'q' || t == 'r' || t == 'b' || t == 'n';
}

// Makes promotion piece match mover color
char normalizePromotionPiece(char promotionPiece, bool white)
{
    char t = tolower(promotionPiece);
    if (white)
        return (char)toupper(t);
    return t;
}

// Copies one board into another
void copyBoard(char src[8][8], char dst[8][8])
{
    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            dst[r][c] = src[r][c];
        }
    }
}

// ----------------------------
// FEN parsing
// ----------------------------

// Parses only the board portion of a FEN string into an 8x8 array
// Empty squares are stored as '.'
bool parseFENBoard(const String &fen, char board[8][8])
{
    String boardPart = fen;

    // If a full FEN is passed in, keep only the first field
    int space = fen.indexOf(' ');
    if (space != -1)
    {
        boardPart = fen.substring(0, space);
    }

    int r = 0;
    int c = 0;

    for (int i = 0; i < boardPart.length(); i++)
    {
        char ch = boardPart[i];

        if (ch == '/')
        {
            if (c != 8)
                return false;

            r++;
            c = 0;

            if (r > 7)
                return false;
        }
        else if (ch >= '1' && ch <= '8')
        {
            int emptyCount = ch - '0';
            for (int k = 0; k < emptyCount; k++)
            {
                if (r > 7 || c > 7)
                    return false;
                board[r][c++] = '.';
            }
        }
        else
        {
            if (r > 7 || c > 7)
                return false;
            board[r][c++] = ch;
        }
    }

    return r == 7 && c == 8;
}

// ----------------------------
// Board validation helpers
// ----------------------------

// Returns true only if board has exactly one white king and one black king
bool boardHasExactlyOneKingEach(char board[8][8])
{
    int whiteKings = 0;
    int blackKings = 0;

    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            if (board[r][c] == 'K')
                whiteKings++;
            if (board[r][c] == 'k')
                blackKings++;
        }
    }

    return whiteKings == 1 && blackKings == 1;
}

// Finds the king of one side
bool findKing(char board[8][8], bool whiteKing, int &kingRow, int &kingCol)
{
    char king = whiteKing ? 'K' : 'k';

    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            if (board[r][c] == king)
            {
                kingRow = r;
                kingCol = c;
                return true;
            }
        }
    }

    return false;
}

// Compares two boards for exact equality
bool sameBoard(char a[8][8], char b[8][8])
{
    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            if (a[r][c] != b[r][c])
                return false;
        }
    }
    return true;
}

// ----------------------------
// Piece movement helpers
// ----------------------------

// Returns true if squares between start and end are empty
// for a horizontal or vertical move
bool clearStraight(char board[8][8], int r1, int c1, int r2, int c2)
{
    if (r1 == r2)
    {
        int step = (c2 > c1) ? 1 : -1;
        for (int c = c1 + step; c != c2; c += step)
        {
            if (board[r1][c] != '.')
                return false;
        }
        return true;
    }

    if (c1 == c2)
    {
        int step = (r2 > r1) ? 1 : -1;
        for (int r = r1 + step; r != r2; r += step)
        {
            if (board[r][c1] != '.')
                return false;
        }
        return true;
    }

    return false;
}

// Returns true if squares between start and end are empty
// for a diagonal move
bool clearDiagonal(char board[8][8], int r1, int c1, int r2, int c2)
{
    int dr = r2 - r1;
    int dc = c2 - c1;

    if (abs(dr) != abs(dc))
        return false;

    int rStep = (dr > 0) ? 1 : -1;
    int cStep = (dc > 0) ? 1 : -1;

    int r = r1 + rStep;
    int c = c1 + cStep;

    while (r != r2)
    {
        if (board[r][c] != '.')
            return false;
        r += rStep;
        c += cStep;
    }

    return true;
}

// Checks whether a pawn move is legal
// This handles normal pawn movement and captures
// Promotion is checked separately
bool validPawnMove(char board[8][8], int r1, int c1, int r2, int c2, char piece)
{
    bool white = isWhitePiece(piece);
    int forward = white ? -1 : 1;
    int startRow = white ? 6 : 1;

    int dr = r2 - r1;
    int dc = c2 - c1;
    char dest = board[r2][c2];

    // Forward one into empty square
    if (dc == 0 && dr == forward && dest == '.')
    {
        return true;
    }

    // Forward two from starting row
    if (dc == 0 &&
        r1 == startRow &&
        dr == 2 * forward &&
        dest == '.' &&
        board[r1 + forward][c1] == '.')
    {
        return true;
    }

    // Diagonal capture
    if (abs(dc) == 1 &&
        dr == forward &&
        dest != '.' &&
        isWhitePiece(dest) != white)
    {
        return true;
    }

    return false;
}

// Knight move check
bool validKnightMove(int r1, int c1, int r2, int c2)
{
    int dr = abs(r2 - r1);
    int dc = abs(c2 - c1);
    return (dr == 2 && dc == 1) || (dr == 1 && dc == 2);
}

// King move check for ordinary king move
bool validKingMove(int r1, int c1, int r2, int c2)
{
    int dr = abs(r2 - r1);
    int dc = abs(c2 - c1);
    return dr <= 1 && dc <= 1 && (dr != 0 || dc != 0);
}

// Applies a normal move to a board
void applyMove(char board[8][8], int r1, int c1, int r2, int c2)
{
    board[r2][c2] = board[r1][c1];
    board[r1][c1] = '.';
}

// Applies a promotion move to a board
void applyPromotion(char board[8][8], int r1, int c1, int r2, int c2, char promotionPiece)
{
    board[r2][c2] = promotionPiece;
    board[r1][c1] = '.';
}

// ----------------------------
// Attack / check helpers
// ----------------------------

// Returns true if a pawn on (r,c) attacks (targetRow,targetCol)
bool pawnAttacksSquare(char board[8][8], int r, int c, int targetRow, int targetCol)
{
    char piece = board[r][c];
    bool white = isWhitePiece(piece);
    int forward = white ? -1 : 1;

    return targetRow == r + forward && abs(targetCol - c) == 1;
}

// Returns true if the piece on (r,c) attacks (targetRow,targetCol)
bool pieceAttacksSquare(char board[8][8], int r, int c, int targetRow, int targetCol)
{
    char piece = board[r][c];
    if (!isPiece(piece))
        return false;

    char type = tolower(piece);

    if (type == 'p')
    {
        return pawnAttacksSquare(board, r, c, targetRow, targetCol);
    }

    if (type == 'n')
    {
        return validKnightMove(r, c, targetRow, targetCol);
    }

    if (type == 'k')
    {
        return validKingMove(r, c, targetRow, targetCol);
    }

    if (type == 'r')
    {
        return clearStraight(board, r, c, targetRow, targetCol);
    }

    if (type == 'b')
    {
        return clearDiagonal(board, r, c, targetRow, targetCol);
    }

    if (type == 'q')
    {
        return clearStraight(board, r, c, targetRow, targetCol) ||
               clearDiagonal(board, r, c, targetRow, targetCol);
    }

    return false;
}

// Returns true if the square is attacked by the chosen side
bool isSquareAttacked(char board[8][8], int targetRow, int targetCol, bool byWhite)
{
    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            char piece = board[r][c];
            if (!isPiece(piece))
                continue;
            if (isWhitePiece(piece) != byWhite)
                continue;

            if (pieceAttacksSquare(board, r, c, targetRow, targetCol))
            {
                return true;
            }
        }
    }

    return false;
}

// Returns true if the given side's king is in check
bool isKingInCheck(char board[8][8], bool whiteKing)
{
    int kingRow, kingCol;

    if (!findKing(board, whiteKing, kingRow, kingCol))
    {
        return true;
    }

    return isSquareAttacked(board, kingRow, kingCol, !whiteKing);
}

// ----------------------------
// Castling helpers
// ----------------------------

// castling[0] = White king-side
// castling[1] = White queen-side
// castling[2] = Black king-side
// castling[3] = Black queen-side

int castleFlagIndex(bool white, bool kingSide)
{
    if (white && kingSide)
        return 0;
    if (white && !kingSide)
        return 1;
    if (!white && kingSide)
        return 2;
    return 3;
}

// Returns true if the given castle is legal in the current board position
bool canCastle(char board[8][8], bool white, bool kingSide, const bool castling[4])
{
    int row = white ? 7 : 0;
    char king = white ? 'K' : 'k';
    char rook = white ? 'R' : 'r';

    int flagIndex = castleFlagIndex(white, kingSide);
    if (!castling[flagIndex])
        return false;

    // King and rook must be on original squares
    if (board[row][4] != king)
        return false;

    if (kingSide)
    {
        if (board[row][7] != rook)
            return false;

        // Squares between king and rook must be empty
        if (board[row][5] != '.' || board[row][6] != '.')
            return false;

        // King cannot castle out of, through, or into check
        if (isSquareAttacked(board, row, 4, !white))
            return false;
        if (isSquareAttacked(board, row, 5, !white))
            return false;
        if (isSquareAttacked(board, row, 6, !white))
            return false;

        return true;
    }
    else
    {
        if (board[row][0] != rook)
            return false;

        // Squares between king and rook must be empty
        if (board[row][1] != '.' || board[row][2] != '.' || board[row][3] != '.')
            return false;

        // King cannot castle out of, through, or into check
        if (isSquareAttacked(board, row, 4, !white))
            return false;
        if (isSquareAttacked(board, row, 3, !white))
            return false;
        if (isSquareAttacked(board, row, 2, !white))
            return false;

        return true;
    }
}

// Applies castling to a board
void applyCastle(char board[8][8], bool white, bool kingSide)
{
    int row = white ? 7 : 0;
    char king = white ? 'K' : 'k';
    char rook = white ? 'R' : 'r';

    // Clear original squares
    board[row][4] = '.';

    if (kingSide)
    {
        board[row][7] = '.';
        board[row][6] = king;
        board[row][5] = rook;
    }
    else
    {
        board[row][0] = '.';
        board[row][2] = king;
        board[row][3] = rook;
    }
}

// ----------------------------
// Move legality helpers
// ----------------------------

// Checks ordinary movement rules only
// This does not include castling
// This does not include "king left in check"
// Promotion legality is handled separately
bool basicMoveIsLegal(char board[8][8], int r1, int c1, int r2, int c2)
{
    char piece = board[r1][c1];
    if (!isPiece(piece))
        return false;

    if (r1 == r2 && c1 == c2)
        return false;
    if (sameColor(piece, board[r2][c2]))
        return false;

    char type = tolower(piece);

    if (type == 'p')
    {
        return validPawnMove(board, r1, c1, r2, c2, piece);
    }

    if (type == 'r')
    {
        return clearStraight(board, r1, c1, r2, c2);
    }

    if (type == 'b')
    {
        return clearDiagonal(board, r1, c1, r2, c2);
    }

    if (type == 'q')
    {
        return clearStraight(board, r1, c1, r2, c2) ||
               clearDiagonal(board, r1, c1, r2, c2);
    }

    if (type == 'n')
    {
        return validKnightMove(r1, c1, r2, c2);
    }

    if (type == 'k')
    {
        return validKingMove(r1, c1, r2, c2);
    }

    return false;
}

// Updates castling rights after a move
void updateCastlingFlags(char before[8][8],
                         int r1, int c1, int r2, int c2,
                         char movedPiece,
                         char capturedPiece,
                         bool oldFlags[4],
                         bool newFlags[4])
{
    for (int i = 0; i < 4; i++)
    {
        newFlags[i] = oldFlags[i];
    }

    // If king moves, both castling rights for that side are lost
    if (movedPiece == 'K')
    {
        newFlags[0] = false;
        newFlags[1] = false;
    }
    if (movedPiece == 'k')
    {
        newFlags[2] = false;
        newFlags[3] = false;
    }

    // If rook moves from its original square, that side loses that castling right
    if (movedPiece == 'R' && r1 == 7 && c1 == 7)
        newFlags[0] = false;
    if (movedPiece == 'R' && r1 == 7 && c1 == 0)
        newFlags[1] = false;
    if (movedPiece == 'r' && r1 == 0 && c1 == 7)
        newFlags[2] = false;
    if (movedPiece == 'r' && r1 == 0 && c1 == 0)
        newFlags[3] = false;

    // If a rook is captured on its original square, that castling right is lost
    if (capturedPiece == 'R' && r2 == 7 && c2 == 7)
        newFlags[0] = false;
    if (capturedPiece == 'R' && r2 == 7 && c2 == 0)
        newFlags[1] = false;
    if (capturedPiece == 'r' && r2 == 0 && c2 == 7)
        newFlags[2] = false;
    if (capturedPiece == 'r' && r2 == 0 && c2 == 0)
        newFlags[3] = false;
}

// Returns true if side to move has at least one legal move
// This includes ordinary moves, promotions, and castling
bool hasAnyLegalMove(char board[8][8], bool whiteToMove, const bool castling[4])
{
    char testBoard[8][8];

    for (int r1 = 0; r1 < 8; r1++)
    {
        for (int c1 = 0; c1 < 8; c1++)
        {
            char piece = board[r1][c1];
            if (!isPiece(piece))
                continue;
            if (isWhitePiece(piece) != whiteToMove)
                continue;

            char type = tolower(piece);

            // Ordinary piece moves
            for (int r2 = 0; r2 < 8; r2++)
            {
                for (int c2 = 0; c2 < 8; c2++)
                {
                    if (!basicMoveIsLegal(board, r1, c1, r2, c2))
                        continue;

                    // Handle promotion possibilities
                    if (type == 'p' && (r2 == 0 || r2 == 7))
                    {
                        char promoChoices[4];
                        if (whiteToMove)
                        {
                            promoChoices[0] = 'Q';
                            promoChoices[1] = 'R';
                            promoChoices[2] = 'B';
                            promoChoices[3] = 'N';
                        }
                        else
                        {
                            promoChoices[0] = 'q';
                            promoChoices[1] = 'r';
                            promoChoices[2] = 'b';
                            promoChoices[3] = 'n';
                        }

                        for (int i = 0; i < 4; i++)
                        {
                            copyBoard(board, testBoard);
                            applyPromotion(testBoard, r1, c1, r2, c2, promoChoices[i]);

                            if (!isKingInCheck(testBoard, whiteToMove))
                            {
                                return true;
                            }
                        }
                    }
                    else
                    {
                        copyBoard(board, testBoard);
                        applyMove(testBoard, r1, c1, r2, c2);

                        if (!isKingInCheck(testBoard, whiteToMove))
                        {
                            return true;
                        }
                    }
                }
            }

            // Castling moves for the king
            if (type == 'k')
            {
                if (canCastle(board, whiteToMove, true, castling))
                {
                    copyBoard(board, testBoard);
                    applyCastle(testBoard, whiteToMove, true);

                    if (!isKingInCheck(testBoard, whiteToMove))
                    {
                        return true;
                    }
                }

                if (canCastle(board, whiteToMove, false, castling))
                {
                    copyBoard(board, testBoard);
                    applyCastle(testBoard, whiteToMove, false);

                    if (!isKingInCheck(testBoard, whiteToMove))
                    {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

// ----------------------------
// Main validator
// ----------------------------

// Returns afterFEN if valid, otherwise returns "Invalid Move"
// castling flag order:
// [0] white king-side
// [1] white queen-side
// [2] black king-side
// [3] black queen-side
String validateMoveAndReturnFEN(const String &beforeFEN,
                                const String &afterFEN,
                                bool whiteToMove,
                                const bool castling[4],
                                char promotionPiece)
{
    char before[8][8];
    char after[8][8];

    if (!parseFENBoard(beforeFEN, before) || !parseFENBoard(afterFEN, after))
    {
        return "Invalid Move";
    }

    if (!boardHasExactlyOneKingEach(before) || !boardHasExactlyOneKingEach(after))
    {
        return "Invalid Move";
    }

    int diffCount = 0;
    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            if (before[r][c] != after[r][c])
                diffCount++;
        }
    }

    bool newCastling[4];
    bool valid = false;

    // ----------------------------
    // Try castling first
    // Castling changes 4 squares
    // ----------------------------
    if (diffCount == 4)
    {
        char expected[8][8];

        // White king-side
        if (whiteToMove && canCastle(before, true, true, castling))
        {
            copyBoard(before, expected);
            applyCastle(expected, true, true);

            if (sameBoard(expected, after))
            {
                for (int i = 0; i < 4; i++)
                    newCastling[i] = castling[i];
                newCastling[0] = false;
                newCastling[1] = false;

                if (!isKingInCheck(after, true))
                {
                    valid = true;
                }
            }
        }

        // White queen-side
        if (!valid && whiteToMove && canCastle(before, true, false, castling))
        {
            copyBoard(before, expected);
            applyCastle(expected, true, false);

            if (sameBoard(expected, after))
            {
                for (int i = 0; i < 4; i++)
                    newCastling[i] = castling[i];
                newCastling[0] = false;
                newCastling[1] = false;

                if (!isKingInCheck(after, true))
                {
                    valid = true;
                }
            }
        }

        // Black king-side
        if (!valid && !whiteToMove && canCastle(before, false, true, castling))
        {
            copyBoard(before, expected);
            applyCastle(expected, false, true);

            if (sameBoard(expected, after))
            {
                for (int i = 0; i < 4; i++)
                    newCastling[i] = castling[i];
                newCastling[2] = false;
                newCastling[3] = false;

                if (!isKingInCheck(after, false))
                {
                    valid = true;
                }
            }
        }

        // Black queen-side
        if (!valid && !whiteToMove && canCastle(before, false, false, castling))
        {
            copyBoard(before, expected);
            applyCastle(expected, false, false);

            if (sameBoard(expected, after))
            {
                for (int i = 0; i < 4; i++)
                    newCastling[i] = castling[i];
                newCastling[2] = false;
                newCastling[3] = false;

                if (!isKingInCheck(after, false))
                {
                    valid = true;
                }
            }
        }
    }

    // ----------------------------
    // Try ordinary move or promotion
    // These change exactly 2 squares
    // ----------------------------
    if (!valid && diffCount == 2)
    {
        int fromRow = -1, fromCol = -1;
        int toRow = -1, toCol = -1;

        for (int r = 0; r < 8; r++)
        {
            for (int c = 0; c < 8; c++)
            {
                if (before[r][c] != after[r][c])
                {
                    // Source square
                    if (before[r][c] != '.' && after[r][c] == '.')
                    {
                        if (fromRow != -1)
                            return "Invalid Move";
                        fromRow = r;
                        fromCol = c;
                    }
                    // Destination square
                    else if (after[r][c] != '.')
                    {
                        if (toRow != -1)
                            return "Invalid Move";
                        toRow = r;
                        toCol = c;
                    }
                }
            }
        }

        if (fromRow == -1 || toRow == -1)
        {
            return "Invalid Move";
        }

        char movedPiece = before[fromRow][fromCol];
        char capturedPiece = before[toRow][toCol];
        char afterDest = after[toRow][toCol];

        if (!isPiece(movedPiece))
            return "Invalid Move";
        if (isWhitePiece(movedPiece) != whiteToMove)
            return "Invalid Move";
        if (sameColor(movedPiece, capturedPiece))
            return "Invalid Move";

        char type = tolower(movedPiece);

        // Must obey movement rules
        if (!basicMoveIsLegal(before, fromRow, fromCol, toRow, toCol))
        {
            return "Invalid Move";
        }

        char expected[8][8];
        copyBoard(before, expected);

        // Handle promotion
        if (type == 'p' && (toRow == 0 || toRow == 7))
        {
            if (!isValidPromotionPiece(promotionPiece))
                return "Invalid Move";

            char normalizedPromo = normalizePromotionPiece(promotionPiece, whiteToMove);

            // Destination must contain the promoted piece, not a pawn
            if (afterDest != normalizedPromo)
                return "Invalid Move";

            applyPromotion(expected, fromRow, fromCol, toRow, toCol, normalizedPromo);
        }
        else
        {
            // Non-promotion move must end with same piece
            if (afterDest != movedPiece)
                return "Invalid Move";

            // Pawn reaching back rank must promote
            if (type == 'p' && (toRow == 0 || toRow == 7))
            {
                return "Invalid Move";
            }

            applyMove(expected, fromRow, fromCol, toRow, toCol);
        }

        // After board must exactly match expected board
        if (!sameBoard(expected, after))
        {
            return "Invalid Move";
        }

        // Cannot leave own king in check
        if (isKingInCheck(after, whiteToMove))
        {
            return "Invalid Move";
        }

        // Update castling rights after this move
        updateCastlingFlags(before,
                            fromRow, fromCol, toRow, toCol,
                            movedPiece, capturedPiece,
                            (bool *)castling, newCastling);

        valid = true;
    }

    if (!valid)
    {
        return "Invalid Move";
    }

    // ----------------------------
    // Keep check / checkmate / stalemate logic
    // even though we only return afterFEN
    // ----------------------------
    bool opponentWhite = !whiteToMove;
    bool opponentInCheck = isKingInCheck(after, opponentWhite);
    bool opponentHasLegalMove = hasAnyLegalMove(after, opponentWhite, newCastling);
    bool opponentCheckmate = opponentInCheck && !opponentHasLegalMove;
    bool opponentStalemate = !opponentInCheck && !opponentHasLegalMove;

    // These are intentionally computed so the logic still exists.
    // We do not return them because you asked to return only afterFEN or Invalid Move.
    (void)opponentInCheck;
    (void)opponentCheckmate;
    (void)opponentStalemate;

    return afterFEN;
}