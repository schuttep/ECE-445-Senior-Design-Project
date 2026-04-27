import json, os, time
import boto3
from botocore.exceptions import ClientError
from decimal import Decimal

TABLE_NAME = os.environ.get("TABLE_NAME")
dynamodb = boto3.resource("dynamodb")
table = dynamodb.Table(TABLE_NAME) if TABLE_NAME else None
SINGLE_GAME_ID = "1"

START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

def _to_full_fen(fen_str, turn_ab):
    """Ensure fen_str is a full FEN with active colour set from turn_ab ('A'=white, 'B'=black).
    Board-only FENs have exactly 1 space-separated part; full FENs have 6.
    Castling/en-passant fields are kept if already present, otherwise defaults are used."""
    parts = fen_str.strip().split()
    active = "w" if turn_ab == "A" else "b"
    if len(parts) == 1:
        return f"{parts[0]} {active} KQkq - 0 1"
    # Already a full FEN — just overwrite the active-colour field
    parts[1] = active
    return " ".join(parts)

# Timer support -----------------------------------------------------------
# Stale threshold: a board is considered disconnected if it hasn't sent a
# heartbeat in this many seconds (boards should heartbeat every ~3 s).
STALE_THRESHOLD_S = 8

# Supported timer modes and their initial time budgets in milliseconds.
TIMER_MODES = {
    "none":   0,
    "rapid":  600_000,   # 10 min / side
    "bullet": 300_000,   #  5 min / side
}


def _compute_timers(item, now_ms):
    """Return (white_ms, black_ms) with the running clock segment applied."""
    white_ms = int(item.get("whiteTimeMs", 0))
    black_ms  = int(item.get("blackTimeMs",  0))
    clock_for     = item.get("clockRunningFor")       # "white" | "black" | absent
    clock_started = int(item.get("clockLastStartedAt", 0))
    if clock_for and clock_started > 0:
        elapsed = now_ms - clock_started
        if clock_for == "white":
            white_ms = max(0, white_ms - elapsed)
        else:
            black_ms  = max(0, black_ms  - elapsed)
    return white_ms, black_ms


def ensure_single_game():
    now = int(time.time())
    item = {
        "gameId": SINGLE_GAME_ID,
        "status": "ACTIVE",
        "fen": START_FEN,
        "turn": "A",
        "version": 0,
        "createdAt": now,
        "lastMoveAt": now,
        "moveHistory": [],
        "whitePlayerId": "",
        "blackPlayerId": "",
        "messages": [],
        # Timer fields — defaults to no timer
        "timerMode": "none",
        "timerInitMs": Decimal(0),
        "whiteTimeMs": Decimal(0),
        "blackTimeMs": Decimal(0),
        "lastSeenWhite": Decimal(0),
        "lastSeenBlack": Decimal(0),
    }

    try:
        table.put_item(Item=item, ConditionExpression="attribute_not_exists(gameId)")
    except ClientError as e:
        if e.response.get("Error", {}).get("Code") != "ConditionalCheckFailedException":
            raise


def extract_games_tail(parts):
    for i in range(len(parts) - 2):
        if parts[i:i + 3] == ["api", "v1", "games"]:
            return parts[i + 3:]
    return None


def resp(status, body=None):
    safe_body = _to_json_safe(body)
    return {
        "statusCode": status,
        "headers": {
            "Content-Type": "application/json",
            "Access-Control-Allow-Origin": "*",
        },
        "body": "" if safe_body is None else json.dumps(safe_body),
    }


def _to_json_safe(value):
    if isinstance(value, Decimal):
        if value % 1 == 0:
            return int(value)
        return float(value)

    if isinstance(value, dict):
        return {k: _to_json_safe(v) for k, v in value.items()}

    if isinstance(value, list):
        return [_to_json_safe(v) for v in value]

    return value


def client_error_payload(prefix, err):
    details = err.response.get("Error", {}) if hasattr(err, "response") else {}
    code = details.get("Code", "CLIENT_ERROR")
    message = details.get("Message", str(err))
    return {
        "error": {
            "code": prefix,
            "message": f"{code}: {message}",
        }
    }


def lambda_handler(event, context):
    if table is None:
        return resp(500, {"error": {"code": "SERVER_CONFIG", "message": "TABLE_NAME environment variable is not set"}})

    method = event.get("requestContext", {}).get("http", {}).get("method")
    path = event.get("requestContext", {}).get("http", {}).get("path", "")
    parts = [p for p in path.split("/") if p]
    tail = extract_games_tail(parts)
    raw_body = event.get("body") or ""
    body = json.loads(raw_body) if raw_body else {}
    query_params = event.get("queryStringParameters") or {}

    # GET /api/v1/games/{gameId}
    if method == "GET" and tail is not None and len(tail) == 1:
        game_id = tail[0]
        if game_id != SINGLE_GAME_ID:
            return resp(404, {"error": {"code": "NOT_FOUND", "message": "game not found"}})

        try:
            ensure_single_game()
            item = table.get_item(Key={"gameId": game_id}).get("Item")
        except ClientError as err:
            return resp(500, client_error_payload("DDB_STATE_READ_FAILED", err))
        except Exception as err:
            return resp(500, {"error": {"code": "STATE_READ_FAILED", "message": str(err)}})

        if not item:
            return resp(404, {"error": {"code": "NOT_FOUND", "message": "game not found"}})

        return resp(200, {
            "gameId": game_id,
            "status": item.get("status"),
            "fen": item.get("fen"),
            "turn": item.get("turn"),
            "version": item.get("version", 0),
            "lastMoveAt": item.get("lastMoveAt"),
            "color": "white" if query_params.get("boardId", "") == item.get("whitePlayerId", "") and item.get("whitePlayerId", "") != "" else "black",
            # Timer fields
            "timerMode": item.get("timerMode", "none"),
            "whiteTimeMs": _compute_timers(item, int(time.time() * 1000))[0],
            "blackTimeMs":  _compute_timers(item, int(time.time() * 1000))[1],
            "clockRunningFor": item.get("clockRunningFor", ""),
        })

    # GET /api/v1/games/{gameId}/messages
    if method == "GET" and tail is not None and len(tail) == 2 and tail[1] == "messages":
        game_id = tail[0]
        if game_id != SINGLE_GAME_ID:
            return resp(404, {"error": {"code": "NOT_FOUND", "message": "game not found"}})

        try:
            ensure_single_game()
            item = table.get_item(Key={"gameId": game_id}).get("Item")
        except ClientError as err:
            return resp(500, client_error_payload("DDB_MESSAGES_READ_FAILED", err))
        except Exception as err:
            return resp(500, {"error": {"code": "MESSAGES_READ_FAILED", "message": str(err)}})

        if not item:
            return resp(404, {"error": {"code": "NOT_FOUND", "message": "game not found"}})

        all_msgs = item.get("messages", [])
        return resp(200, {"messages": all_msgs[-20:] if len(all_msgs) > 20 else all_msgs})

    # POST /api/v1/games/{gameId}/messages
    if method == "POST" and tail is not None and len(tail) == 2 and tail[1] == "messages":
        game_id = tail[0]
        if game_id != SINGLE_GAME_ID:
            return resp(404, {"error": {"code": "NOT_FOUND", "message": "game not found"}})

        board_id = body.get("boardId", "").strip()
        text = body.get("text", "").strip()

        if not board_id:
            return resp(400, {"error": {"code": "BAD_REQUEST", "message": "boardId is required"}})
        if not text:
            return resp(400, {"error": {"code": "BAD_REQUEST", "message": "text is required"}})
        if len(text) > 200:
            return resp(400, {"error": {"code": "BAD_REQUEST", "message": "text exceeds 200 characters"}})

        now = int(time.time())
        msg_entry = {"boardId": board_id[:32], "text": text, "at": now}

        try:
            ensure_single_game()
            table.update_item(
                Key={"gameId": game_id},
                UpdateExpression="SET #msgs=list_append(if_not_exists(#msgs, :empty), :new_msg)",
                ExpressionAttributeNames={"#msgs": "messages"},
                ExpressionAttributeValues={
                    ":empty": [],
                    ":new_msg": [msg_entry],
                },
            )
        except ClientError as err:
            return resp(500, client_error_payload("DDB_MESSAGES_WRITE_FAILED", err))
        except Exception as err:
            return resp(500, {"error": {"code": "MESSAGES_WRITE_FAILED", "message": str(err)}})

        return resp(200, {"accepted": msg_entry})

    # GET /api/v1/games/{gameId}/moves
    if method == "GET" and tail is not None and len(tail) == 2 and tail[1] == "moves":
        game_id = tail[0]
        if game_id != SINGLE_GAME_ID:
            return resp(404, {"error": {"code": "NOT_FOUND", "message": "game not found"}})

        try:
            ensure_single_game()
            item = table.get_item(Key={"gameId": game_id}).get("Item")
        except ClientError as err:
            return resp(500, client_error_payload("DDB_MOVES_READ_FAILED", err))
        except Exception as err:
            return resp(500, {"error": {"code": "MOVES_READ_FAILED", "message": str(err)}})

        if not item:
            return resp(404, {"error": {"code": "NOT_FOUND", "message": "game not found"}})

        return resp(200, {
            "gameId": game_id,
            "version": item.get("version", 0),
            "moves": item.get("moveHistory", []),
        })

    # POST /api/v1/games/{gameId}/moves
    if method == "POST" and tail is not None and len(tail) == 2 and tail[1] == "moves":
        game_id = tail[0]
        if game_id != SINGLE_GAME_ID:
            return resp(404, {"error": {"code": "NOT_FOUND", "message": "game not found"}})

        try:
            ensure_single_game()
        except ClientError as err:
            return resp(500, client_error_payload("DDB_GAME_INIT_FAILED", err))
        except Exception as err:
            return resp(500, {"error": {"code": "GAME_INIT_FAILED", "message": str(err)}})

        move = body.get("move")
        fen_next = body.get("fen")

        if not move:
            return resp(400, {"error": {"code": "BAD_REQUEST", "message": "move is required"}})

        if not fen_next:
            return resp(400, {"error": {"code": "BAD_REQUEST", "message": "fen is required"}})

        try:
            existing = table.get_item(Key={"gameId": game_id}).get("Item")
        except ClientError as err:
            return resp(500, client_error_payload("DDB_GAME_LOAD_FAILED", err))
        except Exception as err:
            return resp(500, {"error": {"code": "GAME_LOAD_FAILED", "message": str(err)}})

        if not existing:
            return resp(404, {"error": {"code": "NOT_FOUND", "message": "game not found"}})

        current_turn = existing.get("turn", "A")
        current_version = int(existing.get("version", 0))
        expected_version = body.get("expectedVersion", current_version)

        try:
            expected_version = int(expected_version)
        except (TypeError, ValueError):
            return resp(400, {"error": {"code": "BAD_REQUEST", "message": "expectedVersion must be an integer"}})

        if expected_version != current_version:
            return resp(409, {
                "error": {"code": "VERSION_CONFLICT", "message": "state version mismatch"},
                "state": {
                    "gameId": game_id,
                    "status": existing.get("status"),
                    "fen": existing.get("fen"),
                    "turn": existing.get("turn"),
                    "version": current_version,
                    "lastMoveAt": existing.get("lastMoveAt"),
                }
            })

        next_turn = "B" if current_turn == "A" else "A"
        next_version = current_version + 1
        now = int(time.time())

        # First move: stamp the sender's boardId as the white player
        board_id = body.get("boardId", "").strip()
        white_player_update = ""
        white_player_values = {}
        if board_id and not existing.get("whitePlayerId", ""):
            white_player_update = ", whitePlayerId=:wpid"
            white_player_values = {":wpid": board_id}

        move_entry = {
            "ply": next_version,
            "turn": current_turn,
            "move": move,
            "at": now,
            "fen": fen_next,
        }

        # ── Timer bookkeeping ────────────────────────────────────────────────
        timer_mode = existing.get("timerMode", "none")
        timer_set_parts = {}
        timer_remove_parts = []

        if timer_mode != "none":
            now_ms = int(time.time() * 1000)
            white_ms, black_ms = _compute_timers(existing, now_ms)

            # Deduct the time used by the side that just moved
            # current_turn "A" = white moved, "B" = black moved
            if current_turn == "A":
                white_ms = max(0, white_ms)   # already computed with deduction
            else:
                black_ms  = max(0, black_ms)

            # Decide whether to start the opponent's clock.
            # Only start it if the opponent board has been seen recently.
            last_seen_white = int(existing.get("lastSeenWhite", 0))
            last_seen_black  = int(existing.get("lastSeenBlack",  0))
            opp_fresh = (
                (next_turn == "A" and last_seen_white > 0 and now - last_seen_white < STALE_THRESHOLD_S) or
                (next_turn == "B" and last_seen_black  > 0 and now - last_seen_black  < STALE_THRESHOLD_S)
            )

            timer_set_parts["whiteTimeMs"]        = Decimal(white_ms)
            timer_set_parts["blackTimeMs"]         = Decimal(black_ms)
            timer_set_parts["clockLastStartedAt"] = Decimal(now_ms if opp_fresh else 0)

            if opp_fresh:
                clock_color = "white" if next_turn == "A" else "black"
                timer_set_parts["clockRunningFor"] = clock_color
            else:
                timer_remove_parts.append("clockRunningFor")
        # ────────────────────────────────────────────────────────────────────

        try:
            # Build timer SET / REMOVE clauses
            timer_set_str = ""
            timer_remove_str = ""
            for k, v in timer_set_parts.items():
                white_player_values[f":{k}"] = v
                timer_set_str += f", {k}=:{k}"
            if timer_remove_parts:
                timer_remove_str = " REMOVE " + ", ".join(timer_remove_parts)

            table.update_item(
                Key={"gameId": game_id},
                UpdateExpression=(
                    "SET #s=:active, #fen=:fen, #turn=:next_turn, #ver=:next_ver, "
                    "lastMoveAt=:t, moveHistory=list_append(if_not_exists(moveHistory, :empty), :new_move)"
                    + white_player_update
                    + timer_set_str
                    + timer_remove_str
                ),
                ConditionExpression="#ver = :expected_ver",
                ExpressionAttributeNames={
                    "#s": "status",
                    "#turn": "turn",
                    "#ver": "version",
                    "#fen": "fen",
                },
                ExpressionAttributeValues={
                    ":active": "ACTIVE",
                    ":expected_ver": expected_version,
                    ":next_turn": next_turn,
                    ":next_ver": next_version,
                    ":fen": fen_next,
                    ":t": now,
                    ":empty": [],
                    ":new_move": [move_entry],
                    **white_player_values,
                },
            )
        except ClientError as e:
            if e.response.get("Error", {}).get("Code") == "ConditionalCheckFailedException":
                latest = table.get_item(Key={"gameId": game_id}).get("Item") or {}
                return resp(409, {
                    "error": {"code": "VERSION_CONFLICT", "message": "concurrent update conflict"},
                    "state": {
                        "gameId": game_id,
                        "status": latest.get("status"),
                        "fen": latest.get("fen"),
                        "turn": latest.get("turn"),
                        "version": latest.get("version", 0),
                        "lastMoveAt": latest.get("lastMoveAt"),
                    }
                })
            raise

        # ── AI auto-reply ────────────────────────────────────────────────────
        # If this is an AI game and the human (white, turn "A") just moved,
        # immediately compute and commit Stockfish's response so the board
        # sees it on the next poll without waiting for a second player.
        game_mode = existing.get("gameMode", "pvp")
        if game_mode == "ai" and current_turn == "A":   # human just moved as white
            try:
                import chess
                import chess.engine
                # next_turn is "B" (black to move) — build a full FEN with correct active colour
                ai_full_fen = _to_full_fen(fen_next, "B")
                ai_board = chess.Board(ai_full_fen)
                with chess.engine.SimpleEngine.popen_uci("/opt/bin/stockfish") as engine:
                    engine.configure({"Hash": 16})
                    ai_result = engine.play(ai_board, chess.engine.Limit(depth=5))
                ai_uci = ai_result.move.uci()
                ai_board.push(ai_result.move)
                ai_fen = ai_board.fen()
                ai_version  = next_version + 1
                ai_move_entry = {
                    "ply":  ai_version,
                    "turn": "B",
                    "move": ai_uci,
                    "at":   now,
                    "fen":  ai_fen,
                }
                table.update_item(
                    Key={"gameId": game_id},
                    UpdateExpression=(
                        "SET #s=:active, #fen=:fen, #turn=:next_turn, #ver=:next_ver, "
                        "lastMoveAt=:t, "
                        "moveHistory=list_append(if_not_exists(moveHistory, :empty), :new_move)"
                    ),
                    ExpressionAttributeNames={
                        "#s": "status",
                        "#turn": "turn",
                        "#ver": "version",
                        "#fen": "fen",
                    },
                    ExpressionAttributeValues={
                        ":active":    "ACTIVE",
                        ":next_turn": "A",
                        ":next_ver":  ai_version,
                        ":fen":       ai_fen,
                        ":t":         now,
                        ":empty":     [],
                        ":new_move":  [ai_move_entry],
                    },
                )
                return resp(200, {
                    "gameId":      game_id,
                    "status":      "ACTIVE",
                    "fen":         fen_next,
                    "turn":        next_turn,
                    "version":     next_version,
                    "lastMoveAt":  now,
                    "acceptedMove": move_entry,
                    "aiMove":      ai_uci,
                    "aiFen":       ai_fen,
                })
            except Exception as ai_err:
                # AI move failed — fall through and return the human move as normal.
                # The board will just wait as if it's the opponent's turn.
                pass
        # ────────────────────────────────────────────────────────────────────

        return resp(200, {
            "gameId": game_id,
            "status": "ACTIVE",
            "fen": fen_next,
            "turn": next_turn,
            "version": next_version,
            "lastMoveAt": now,
            "acceptedMove": move_entry,
        })

    # POST /api/v1/games/{gameId}/reset
    if method == "POST" and tail is not None and len(tail) == 2 and tail[1] == "reset":
        game_id = tail[0]
        if game_id != SINGLE_GAME_ID:
            return resp(404, {"error": {"code": "NOT_FOUND", "message": "game not found"}})

        timer_mode = body.get("timerMode", "none")
        if timer_mode not in TIMER_MODES:
            timer_mode = "none"
        timer_init_ms    = TIMER_MODES[timer_mode]
        creator_board_id = body.get("boardId", "").strip()[:32]
        game_mode        = "ai" if body.get("gameMode") == "ai" else "pvp"

        try:
            now = int(time.time())
            table.update_item(
                Key={"gameId": game_id},
                UpdateExpression=(
                    "SET #s=:active, #fen=:fen, #turn=:turn, #ver=:ver, "
                    "lastMoveAt=:t, moveHistory=:history, "
                    "whitePlayerId=:wpid, blackPlayerId=:bpid, #msgs=:no_msgs, "
                    "timerMode=:tm, timerInitMs=:ti, "
                    "whiteTimeMs=:wt, blackTimeMs=:bt, "
                    "clockLastStartedAt=:zero, "
                    "lastSeenWhite=:lsw, lastSeenBlack=:lsb, "
                    "gameMode=:gm "
                    "REMOVE clockRunningFor"
                ),
                ExpressionAttributeNames={
                    "#s": "status",
                    "#fen": "fen",
                    "#turn": "turn",
                    "#ver": "version",
                    "#msgs": "messages",
                },
                ExpressionAttributeValues={
                    ":active": "ACTIVE",
                    ":fen": START_FEN,
                    ":turn": "A",
                    ":ver": 0,
                    ":t": now,
                    ":history": [],
                    ":wpid": creator_board_id,
                    ":bpid": "",
                    ":no_msgs": [],
                    ":tm": timer_mode,
                    ":ti": Decimal(timer_init_ms),
                    ":wt": Decimal(timer_init_ms),
                    ":bt": Decimal(timer_init_ms),
                    ":zero": Decimal(0),
                    ":lsw": Decimal(now if creator_board_id else 0),
                    ":lsb": Decimal(0),
                    ":gm": game_mode,
                },
            )
        except ClientError as err:
            return resp(500, client_error_payload("DDB_RESET_FAILED", err))
        except Exception as err:
            return resp(500, {"error": {"code": "RESET_FAILED", "message": str(err)}})

        return resp(200, {
            "gameId": game_id,
            "status": "ACTIVE",
            "fen": START_FEN,
            "turn": "A",
            "version": 0,
            "lastMoveAt": now,
            "movesCleared": True,
            "timerMode": timer_mode,
            "gameMode": game_mode,
        })

    # POST /api/v1/games/{gameId}/heartbeat
    if method == "POST" and tail is not None and len(tail) == 2 and tail[1] == "heartbeat":
        game_id = tail[0]
        if game_id != SINGLE_GAME_ID:
            return resp(404, {"error": {"code": "NOT_FOUND", "message": "game not found"}})

        board_id = body.get("boardId", "").strip()[:32]
        if not board_id:
            return resp(400, {"error": {"code": "BAD_REQUEST", "message": "boardId is required"}})

        try:
            item = table.get_item(Key={"gameId": game_id}).get("Item")
            if not item:
                return resp(404, {"error": {"code": "NOT_FOUND", "message": "game not found"}})

            now     = int(time.time())
            now_ms  = now * 1000

            white_id = item.get("whitePlayerId", "")
            black_id  = item.get("blackPlayerId",  "")
            timer_mode = item.get("timerMode", "none")
            timer_init_ms = int(item.get("timerInitMs", 0))
            clock_for     = item.get("clockRunningFor")
            turn          = item.get("turn", "A")

            last_seen_white = int(item.get("lastSeenWhite", 0))
            last_seen_black  = int(item.get("lastSeenBlack",  0))

            set_parts    = []
            remove_parts = []
            expr_values  = {}

            # Identify the sender and update their last-seen timestamp
            is_white  = (board_id == white_id and white_id != "")
            is_black  = (board_id == black_id  and black_id  != "")
            is_new_black = (not is_white and not is_black
                            and black_id == "" and white_id != "")

            if is_white:
                set_parts.append("lastSeenWhite=:lsw")
                expr_values[":lsw"] = Decimal(now)
                last_seen_white = now
            elif is_new_black:
                set_parts.append("blackPlayerId=:bpid")
                set_parts.append("lastSeenBlack=:lsb")
                expr_values[":bpid"] = board_id
                expr_values[":lsb"]  = Decimal(now)
                black_id      = board_id
                last_seen_black = now
            elif is_black:
                set_parts.append("lastSeenBlack=:lsb")
                expr_values[":lsb"] = Decimal(now)
                last_seen_black = now

            # ── Timer clock management ─────────────────────────────────────
            if timer_mode != "none" and timer_init_ms > 0 and white_id and black_id:
                white_fresh = (last_seen_white > 0
                               and now - last_seen_white < STALE_THRESHOLD_S)
                black_fresh  = (last_seen_black  > 0
                                and now - last_seen_black  < STALE_THRESHOLD_S)
                both_connected = white_fresh and black_fresh

                white_ms, black_ms = _compute_timers(item, now_ms)

                if both_connected and clock_for is None:
                    # Both boards online — resume clock for the side to move
                    clock_color = "white" if turn == "A" else "black"
                    set_parts.append("clockRunningFor=:crf")
                    set_parts.append("clockLastStartedAt=:clsa")
                    set_parts.append("whiteTimeMs=:wtm")
                    set_parts.append("blackTimeMs=:btm")
                    expr_values[":crf"]  = clock_color
                    expr_values[":clsa"] = Decimal(now_ms)
                    expr_values[":wtm"]  = Decimal(white_ms)
                    expr_values[":btm"]  = Decimal(black_ms)
                elif not both_connected and clock_for is not None:
                    # A board went away — pause clock and snapshot remaining times
                    remove_parts.append("clockRunningFor")
                    set_parts.append("clockLastStartedAt=:zero")
                    set_parts.append("whiteTimeMs=:wtm")
                    set_parts.append("blackTimeMs=:btm")
                    expr_values[":zero"] = Decimal(0)
                    expr_values[":wtm"]  = Decimal(white_ms)
                    expr_values[":btm"]  = Decimal(black_ms)

            if set_parts or remove_parts:
                expr = ""
                if set_parts:
                    expr = "SET " + ", ".join(set_parts)
                if remove_parts:
                    expr += (" " if expr else "") + "REMOVE " + ", ".join(remove_parts)
                table.update_item(
                    Key={"gameId": game_id},
                    UpdateExpression=expr,
                    ExpressionAttributeValues=expr_values,
                )
        except ClientError as err:
            return resp(500, client_error_payload("DDB_HEARTBEAT_FAILED", err))
        except Exception as err:
            return resp(500, {"error": {"code": "HEARTBEAT_FAILED", "message": str(err)}})

        return resp(200, {"ok": True})

    # POST /api/v1/games/{gameId}/hint
    if method == "POST" and tail is not None and len(tail) == 2 and tail[1] == "hint":
        game_id = tail[0]
        if game_id != SINGLE_GAME_ID:
            return resp(404, {"error": {"code": "NOT_FOUND", "message": "game not found"}})

        board_id = body.get("boardId", "").strip()[:32]
        fen      = body.get("fen", "").strip()

        if not board_id:
            return resp(400, {"error": {"code": "BAD_REQUEST", "message": "boardId is required"}})
        if not fen:
            return resp(400, {"error": {"code": "BAD_REQUEST", "message": "fen is required"}})

        try:
            ensure_single_game()
            item = table.get_item(Key={"gameId": game_id}).get("Item")
        except ClientError as err:
            return resp(500, client_error_payload("DDB_HINT_READ_FAILED", err))
        except Exception as err:
            return resp(500, {"error": {"code": "HINT_READ_FAILED", "message": str(err)}})

        if not item:
            return resp(404, {"error": {"code": "NOT_FOUND", "message": "game not found"}})

        white_id = item.get("whitePlayerId", "")
        black_id  = item.get("blackPlayerId",  "")
        is_white  = (board_id == white_id and white_id != "")
        is_black  = (board_id == black_id  and black_id  != "")

        if not is_white and not is_black:
            return resp(403, {"error": {"code": "NOT_A_PLAYER",
                                        "message": "boardId is not a registered player in this game"}})

        # Run Stockfish via python-chess
        # Use the server's turn field to set the correct active colour so
        # Stockfish plays for the right side (board-only FENs default to white).
        db_turn = item.get("turn", "A")
        full_fen = _to_full_fen(fen, db_turn)
        try:
            import chess
            import chess.engine
            board = chess.Board(full_fen)
            with chess.engine.SimpleEngine.popen_uci("/opt/bin/stockfish") as engine:
                engine.configure({"Hash": 16})  # cap hash table to stay within Lambda memory
                result = engine.play(board, chess.engine.Limit(depth=5))
            best_move = result.move.uci()   # e.g. "e2e4"
            board.push(result.move)
            after_fen = board.board_fen()   # board-only FEN after hint move
        except Exception as err:
            return resp(500, {"error": {"code": "STOCKFISH_ERROR", "message": str(err)}})

        color_name = "White" if is_white else "Black"
        msg_text   = f"{color_name} used Stockfish assistance."
        now        = int(time.time())
        msg_entry  = {"boardId": board_id, "text": msg_text, "at": now}

        try:
            table.update_item(
                Key={"gameId": game_id},
                UpdateExpression=(
                    "SET #msgs=list_append(if_not_exists(#msgs, :empty), :new_msg)"
                ),
                ExpressionAttributeNames={"#msgs": "messages"},
                ExpressionAttributeValues={
                    ":empty":   [],
                    ":new_msg": [msg_entry],
                },
            )
        except ClientError as err:
            return resp(500, client_error_payload("DDB_HINT_WRITE_FAILED", err))
        except Exception as err:
            return resp(500, {"error": {"code": "HINT_WRITE_FAILED", "message": str(err)}})

        return resp(200, {
            "move":     best_move,
            "afterFen": after_fen,
        })

    return resp(404, {"error": {"code": "NOT_FOUND", "message": "route not found"}})