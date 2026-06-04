#!/usr/bin/env python3
"""Persistent Python executor for OmniStudio SMD cells.

Provides Jupyter-like cell execution: each cell's stdout/stderr is captured
independently, while variables, imports, and functions share a persistent
namespace across executions.

Communicates via stdin/stdout JSON lines protocol (same pattern as
completion_helper.py):

Request:  {"action":"exec|reset|exit","code":"..."}
Response: {"ok":true,"stdout":"...","stderr":"..."}  or  {"ok":false,"error":"..."}
"""

import sys
import io
import json
import base64
import traceback


def main():
    namespace = {}

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue

        try:
            req = json.loads(line)
        except json.JSONDecodeError as e:
            reply = {"ok": False, "error": f"invalid JSON: {e}"}
            print(json.dumps(reply, ensure_ascii=False), flush=True)
            continue

        action = req.get("action", "")

        if action == "exec":
            code_b64 = req.get("code", "")
            if not code_b64:
                reply = {"ok": True, "stdout": "", "stderr": ""}
                print(json.dumps(reply, ensure_ascii=False), flush=True)
                continue

            # Decode base64 (avoids JSON newline escaping issues)
            try:
                code = base64.b64decode(code_b64).decode("utf-8")
            except Exception:
                reply = {"ok": False, "error": "base64 decode failed"}
                print(json.dumps(reply, ensure_ascii=False), flush=True)
                continue

            # Sanitize lone surrogates that may come from Qt (UTF-16) strings
            try:
                code = code.encode("utf-8", errors="replace").decode("utf-8")
            except Exception:
                pass

            old_stdout = sys.stdout
            old_stderr = sys.stderr
            sys.stdout = io.StringIO()
            sys.stderr = io.StringIO()

            error = None
            try:
                compiled = compile(code, "<cell>", "exec")
                exec(compiled, namespace)
            except Exception:
                error = traceback.format_exc()

            stdout_captured = sys.stdout.getvalue()
            stderr_captured = sys.stderr.getvalue()
            sys.stdout = old_stdout
            sys.stderr = old_stderr

            reply = {"ok": True, "stdout": stdout_captured, "stderr": stderr_captured}
            if error:
                reply["ok"] = False
                reply["error"] = error

        elif action == "reset":
            namespace.clear()
            reply = {"ok": True}

        elif action == "exit":
            reply = {"ok": True}
            print(json.dumps(reply, ensure_ascii=False), flush=True)
            break

        else:
            reply = {"ok": False, "error": f"unknown action: {action}"}

        print(json.dumps(reply, ensure_ascii=False), flush=True)


if __name__ == "__main__":
    main()
