#!/usr/bin/env python3
"""Jedi-based completion helper for Smart-Markdown.

Communicates via stdin/stdout JSON lines protocol:

Request:  {"action":"complete|hover|signature","code":"...","cursor":[line,col]}
Response: {"ok":true,"data":...}  or  {"ok":false,"error":"..."}

line/col are 0-based.
"""

import sys
import json
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
        reply = {"ok": False, "error": "jedi is not installed. Run: pip install jedi"}
        print(json.dumps(reply, ensure_ascii=False), flush=True)
        # Keep reading but reply with error for every request
        for line in sys.stdin:
            if not line.strip():
                continue
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
            script = jedi.Script(code=code, path="untitled.py")
            if action == "complete":
                data = handle_complete(script, line_1based, col)
            elif action == "hover":
                data = handle_hover(script, line_1based, col)
            elif action == "signature":
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
