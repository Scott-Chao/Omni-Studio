#!/usr/bin/env python3
"""Jedi-based completion helper for Smart-Markdown.

Communicates via stdin/stdout JSON lines protocol:

Request:  {"action":"complete|hover|signature","code":"...","cursor":[line,col]}
Response: {"ok":true,"data":...}  or  {"ok":false,"error":"..."}

line/col are 0-based.
"""

import sys
import json
import base64
import traceback

try:
    import jedi
except ImportError:
    jedi = None


def handle_complete(script, line, col):
    """Return list of completion items matching CompletionItem struct."""
    completions = script.complete(line, col)
    items = []
    for c in completions:
        item = {
            "name": c.name,
            "type": _jedi_type_to_string(c.type),
            "signature": c.description or c.name,
            "detail": c.module_name or "",
            "doc": c.docstring() or "",
        }
        items.append(item)
    return items


def handle_hover(script, line, col):
    """Return hover info matching HoverInfo struct."""
    signatures = script.get_signatures(line, col)
    if signatures:
        sig = signatures[0]
        info = {
            "signature": sig.description or "",
            "doc": sig.docstring() or "",
            "definition": sig.module_name or "",
        }
        return info

    # Fallback: use inferred type
    names = script.infer(line, col)
    if names:
        n = names[0]
        info = {
            "signature": f"{n.name}: {n.type}" if n.type else n.name,
            "doc": n.docstring() or "",
            "definition": n.module_name or "",
        }
        return info

    return {"signature": "", "doc": "", "definition": ""}


def handle_diagnostics(cells):
    """Return diagnostics for a list of cells.

    Each cell is {cellIndex, code}. Code is compiled in isolation so line
    numbers are cell-local (0-based) — no virtual-document offset needed.
    Returns list of dicts: {cellIndex, startLine, startCol, endLine, endCol,
                             message, severity}
    Severity: 1=Error, 2=Warning.
    """
    diagnostics = []

    for cell in cells:
        cell_index = cell.get("cellIndex", -1)
        code_b64 = cell.get("code", "")
        if not code_b64:
            continue

        # Decode base64 (avoids JSON newline escaping issues)
        try:
            code = base64.b64decode(code_b64).decode("utf-8")
        except Exception:
            continue

        # Sanitize lone surrogates that may come from Qt (UTF-16) strings.
        code = code.encode("utf-8", errors="replace").decode("utf-8")

        try:
            compile(code, f"<cell {cell_index}>", "exec")
        except SyntaxError as e:
            start_line = (e.lineno or 1) - 1  # 0-based, cell-local
            start_col = (e.offset or 1) - 1   # 0-based
            end_line = start_line
            end_col = start_col + 1
            if e.text:
                end_col = len(e.text.rstrip())

            diagnostics.append({
                "cellIndex": cell_index,
                "startLine": start_line,
                "startCol": start_col,
                "endLine": end_line,
                "endCol": end_col,
                "message": f"SyntaxError: {e.msg}",
                "severity": 1,
            })
        except (UnicodeEncodeError, ValueError):
            pass

    return diagnostics


def handle_signature(script, line, col):
    """Return list of signature info matching SignatureInfo struct."""
    signatures = script.get_signatures(line, col)
    sigs = []
    for sig in signatures:
        info = {
            "label": sig.description or "",
            "doc": sig.docstring() or "",
            "parameters": [p.description for p in sig.params],
            "activeParameter": sig.index if hasattr(sig, "index") else 0,
        }
        sigs.append(info)

    # If no signature but the cursor is after '(', try completing to
    # offer callable completions as a fallback.
    return sigs


def _jedi_type_to_string(jedi_type):
    mapping = {
        "function": "function",
        "class": "class",
        "module": "module",
        "instance": "variable",
        "statement": "variable",
        "keyword": "keyword",
        "param": "variable",
        "property": "property",
    }
    return mapping.get(jedi_type, "text")


def main():
    if jedi is None:
        # Diagnostics work without Jedi; other actions need it.
        print(json.dumps({"ok": False, "error": "jedi is not installed. Run: pip install jedi"},
                         ensure_ascii=False), flush=True)
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
            if action == "diagnostics":
                cells = req.get("cells", [])
                reply = {"ok": True, "data": handle_diagnostics(cells)}
            else:
                reply = {"ok": False, "error": "jedi is not installed"}
            print(json.dumps(reply, ensure_ascii=False), flush=True)
        return

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

        action = req.get("action")
        code = req.get("code", "")
        cursor = req.get("cursor", [0, 0])
        # Cursor line from C++ side is 0-based; Jedi uses 1-based.
        line_idx, col = cursor if len(cursor) == 2 else (0, 0)
        line_1based = line_idx + 1

        try:
            if action == "diagnostics":
                cells = req.get("cells", [])
                data = handle_diagnostics(cells)
            elif action in ("complete", "hover", "signature"):
                script = jedi.Script(code=code, path="untitled.py")
                if action == "complete":
                    data = handle_complete(script, line_1based, col)
                elif action == "hover":
                    data = handle_hover(script, line_1based, col)
                else:
                    data = handle_signature(script, line_1based, col)
            else:
                reply = {"ok": False, "error": f"unknown action: {action}"}
                print(json.dumps(reply, ensure_ascii=False), flush=True)
                continue

            reply = {"ok": True, "data": data}
        except Exception as e:
            reply = {"ok": False, "error": traceback.format_exc()}

        print(json.dumps(reply, ensure_ascii=False), flush=True)


if __name__ == "__main__":
    main()
