import json, os, time
import boto3
from botocore.exceptions import ClientError
from decimal import Decimal

TABLE_NAME = os.environ.get("TABLE_NAME")
dynamodb = boto3.resource("dynamodb")
table = dynamodb.Table(TABLE_NAME) if TABLE_NAME else None
SINGLE_GAME_ID = "1"

START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"


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
        })

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

        try:
            table.update_item(
                Key={"gameId": game_id},
                UpdateExpression=(
                    "SET #s=:active, #fen=:fen, #turn=:next_turn, #ver=:next_ver, "
                    "lastMoveAt=:t, moveHistory=list_append(if_not_exists(moveHistory, :empty), :new_move)"
                    + white_player_update
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

        try:
            now = int(time.time())
            table.update_item(
                Key={"gameId": game_id},
                UpdateExpression=(
                    "SET #s=:active, #fen=:fen, #turn=:turn, #ver=:ver, "
                    "lastMoveAt=:t, moveHistory=:history, "
                    "whitePlayerId=:empty"
                ),
                ExpressionAttributeNames={
                    "#s": "status",
                    "#fen": "fen",
                    "#turn": "turn",
                    "#ver": "version",
                },
                ExpressionAttributeValues={
                    ":active": "ACTIVE",
                    ":fen": START_FEN,
                    ":turn": "A",
                    ":ver": 0,
                    ":t": now,
                    ":history": [],
                    ":empty": "",
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
        })

    return resp(404, {"error": {"code": "NOT_FOUND", "message": "route not found"}})