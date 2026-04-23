#pragma once

#include <Arduino.h>

// Basic piece helpers
bool isWhitePiece(char p);
bool isPiece(char p);
bool sameColor(char a, char b);
bool isValidPromotionPiece(char p);
char normalizePromotionPiece(char promotionPiece, bool white);
void copyBoard(char src[8][8], char dst[8][8]);

// FEN parsing
bool parseFENBoard(const String &fen, char board[8][8]);

// Board validation helpers
bool boardHasExactlyOneKingEach(char board[8][8]);
bool findKing(char board[8][8], bool whiteKing, int &kingRow, int &kingCol);
bool sameBoard(char a[8][8], char b[8][8]);

// Piece movement helpers
bool clearStraight(char board[8][8], int r1, int c1, int r2, int c2);
bool clearDiagonal(char board[8][8], int r1, int c1, int r2, int c2);
bool validPawnMove(char board[8][8], int r1, int c1, int r2, int c2, char piece);
bool validKnightMove(int r1, int c1, int r2, int c2);
bool validKingMove(int r1, int c1, int r2, int c2);
void applyMove(char board[8][8], int r1, int c1, int r2, int c2);
void applyPromotion(char board[8][8], int r1, int c1, int r2, int c2, char promotionPiece);

// Attack / check helpers
bool pawnAttacksSquare(char board[8][8], int r, int c, int targetRow, int targetCol);
bool pieceAttacksSquare(char board[8][8], int r, int c, int targetRow, int targetCol);
bool isSquareAttacked(char board[8][8], int targetRow, int targetCol, bool byWhite);
bool isKingInCheck(char board[8][8], bool whiteKing);

// Castling helpers
// castling[0] = White king-side
// castling[1] = White queen-side
// castling[2] = Black king-side
// castling[3] = Black queen-side
int castleFlagIndex(bool white, bool kingSide);
bool canCastle(char board[8][8], bool white, bool kingSide, const bool castling[4]);
void applyCastle(char board[8][8], bool white, bool kingSide);

// Move legality helpers
bool basicMoveIsLegal(char board[8][8], int r1, int c1, int r2, int c2);
void updateCastlingFlags(char before[8][8],
                         int r1, int c1, int r2, int c2,
                         char movedPiece, char capturedPiece,
                         bool oldFlags[4], bool newFlags[4]);
bool hasAnyLegalMove(char board[8][8], bool whiteToMove, const bool castling[4],
                     const char *enPassantSquare = nullptr);

// Main validator
// enPassantSquare: en-passant target square in algebraic notation (e.g. "e3"),
// or nullptr / empty string if en passant is not available this move.
String validateMoveAndReturnFEN(const String &beforeFEN,
                                const String &afterFEN,
                                bool whiteToMove,
                                const bool castling[4],
                                char promotionPiece,
                                const char *enPassantSquare = nullptr);
