#!/usr/bin/env python3
"""Jedi-based completion helper for Smart-Markdown.

Communicates via stdin/stdout JSON lines protocol:

Request:  {"action":"complete|hover|signature|tokens","code":"...","cursor":[line,col]}
Response: {"ok":true,"data":...}  or  {"ok":false,"error":"..."}

line/col are 0-based.
"""

import sys
import json
import re
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
    # Try infer() first so variable types aren't shadowed by
    # get_signatures() when the cursor lands inside a call's parens.
    names = script.infer(line, col)
    inferred_sig = ""
    inferred_doc = ""
    inferred_def = ""
    is_function = False
    if names:
        n = names[0]
        inferred_sig = f"{n.name}: {n.type}" if n.type else n.name
        inferred_doc = n.docstring() or ""
        inferred_def = n.module_name or ""
        is_function = (n.type == "function")

    # For function calls, also try get_signatures() to get parameter info.
    signatures = script.get_signatures(line, col)
    if signatures:
        sig = signatures[0]
        if is_function:
            # Prefer the full signature over the bare "name: function"
            return {
                "signature": sig.description or inferred_sig,
                "doc": sig.docstring() or inferred_doc,
                "definition": sig.module_name or inferred_def,
            }
        # Not a function — use inferred type; add signature params as extra
        # context only when the cursor is inside a call.
        info = {
            "signature": inferred_sig,
            "doc": inferred_doc,
            "definition": inferred_def,
        }
        if sig.description:
            info["signature"] = inferred_sig + "\n\n" + sig.description
        return info

    if names:
        return {
            "signature": inferred_sig,
            "doc": inferred_doc,
            "definition": inferred_def,
        }

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


def handle_tokens(script, code):
    """Return list of semantic tokens — definitions AND all reference occurrences.

    Jedi's get_names() only returns definitions, but for highlighting we want
    every occurrence of a named identifier.  We collect the type for each
    defined name and then use regex word-boundary search to find every usage
    in the source (including the definition itself).
    """
    names = script.get_names(all_scopes=True)
    if not names:
        return []

    # 1. Collect (name_str, type) for each definition, keeping the most
    #    specific type when the same name is defined in multiple scopes.
    TYPE_PRIORITY = {
        "function": 6,
        "class": 5,
        "parameter": 4,
        "property": 3,
        "variable": 2,
        "module": 1,
    }
    name_type = {}  # name_str → (priority, type)
    for n in names:
        token_type = _jedi_type_to_string(n.type)
        # Keywords are already handled by the regex highlighter
        if token_type == "keyword":
            continue
        prio = TYPE_PRIORITY.get(token_type, 0)
        old = name_type.get(n.name)
        if old is None or prio > old[0]:
            name_type[n.name] = (prio, token_type)

    # 2. Regex-scan the source for every occurrence of each name.
    code_lines = code.split('\n')
    tokens = []
    seen = set()
    for name_str, (_prio, ttype) in name_type.items():
        escaped = re.escape(name_str)
        pattern = re.compile(r'\b' + escaped + r'\b')
        for line_idx, line_text in enumerate(code_lines):
            for m in pattern.finditer(line_text):
                col_0based = m.start()
                key = (line_idx, col_0based)
                if key in seen:
                    continue
                seen.add(key)
                tokens.append({
                    "line": line_idx,
                    "startChar": col_0based,
                    "length": len(name_str),
                    "type": ttype,
                })
    return tokens


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
        "param": "parameter",
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
            elif action == "tokens":
                # code is base64-encoded to avoid JSON escaping issues
                try:
                    code = base64.b64decode(code).decode("utf-8")
                except Exception:
                    code = ""
                script = jedi.Script(code=code, path="untitled.py")
                data = handle_tokens(script, code)
            elif action in ("complete", "hover", "signature"):
                # Hover sends cell-local code as base64; completion/signature
                # send the full virtual document as plain text.  Auto-detect.
                try:
                    code = base64.b64decode(code).decode("utf-8")
                except Exception:
                    pass  # plain-text virtual document (completion/signature)
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
