#!/usr/bin/env python3
"""Jedi-based completion helper for OmniStudio.

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


def handle_hover(script, code, line, col):
    """Return hover info matching HoverInfo struct."""
    # Detect whether the cursor is on a keyword argument (name, =, or value).
    kw = _keyword_at_position(code, line, col)

    names = script.infer(line, col)
    if names:
        n = names[0]
        # When infer() returns a function/method but the cursor is on a
        # keyword argument, show parameter-level info from the signature.
        if kw is not None and n.type in ("function", "method"):
            param_info = _param_info_for_keyword(script, line, col, kw)
            if param_info:
                return param_info

        info = {
            "signature": f"{n.name}: {n.type}" if n.type else n.name,
            "doc": n.docstring() or "",
            "definition": n.module_name or "",
        }
        return info

    # Cursor is on a keyword argument but infer() returned nothing —
    # still try to show parameter info from the enclosing call's signature.
    if kw is not None:
        param_info = _param_info_for_keyword(script, line, col, kw)
        if param_info:
            return param_info

    # Fallback: use function signature
    signatures = script.get_signatures(line, col)
    if signatures:
        sig = signatures[0]
        info = {
            "signature": sig.description or "",
            "doc": sig.docstring() or "",
            "definition": sig.module_name or "",
        }
        return info

    return {"signature": "", "doc": "", "definition": ""}


def _keyword_at_position(source, line_1based, col):
    """If col falls inside a ``keyword=value`` argument, return the keyword name.

    line_1based is Jedi's 1-based line number; col is 0-based.
    Returns None when the position is not inside a keyword argument.
    """
    lines = source.split("\n")
    line_idx = line_1based - 1
    if line_idx < 0 or line_idx >= len(lines):
        return None
    line_text = lines[line_idx]
    if col < 0 or col >= len(line_text):
        return None

    c = line_text[col]

    # Case 1: cursor is on an identifier — check if it is followed by '='
    if c.isalnum() or c == "_":
        end = col
        while end < len(line_text) and (line_text[end].isalnum() or line_text[end] == "_"):
            end += 1
        rest = line_text[end:].lstrip()
        if rest and rest[0] == "=":
            start = col
            while start > 0 and (line_text[start - 1].isalnum() or line_text[start - 1] == "_"):
                start -= 1
            return line_text[start:end]

        # Check if preceded by '=' (cursor on the value)
        before = line_text[:col].rstrip()
        if before and before[-1] == "=":
            return _extract_kw_name_before(line_text, len(before) - 1)
        return None

    # Case 2: cursor is directly on '='
    if c == "=":
        return _extract_kw_name_before(line_text, col - 1)

    # Case 3: cursor is on whitespace / other chars — search both sides
    # Look left for '='
    scan = col - 1
    while scan >= 0 and line_text[scan].isspace():
        scan -= 1
    if scan >= 0 and line_text[scan] == "=":
        return _extract_kw_name_before(line_text, scan - 1)
    # Look right for '='
    scan = col
    while scan < len(line_text) and line_text[scan].isspace():
        scan += 1
    if scan < len(line_text) and line_text[scan] == "=":
        return _extract_kw_name_before(line_text, col - 1)

    return None


def _extract_kw_name_before(line_text, end_pos):
    """Extract an identifier ending at *end_pos*, scanning left past whitespace."""
    end = end_pos
    while end >= 0 and line_text[end].isspace():
        end -= 1
    if end < 0 or not (line_text[end].isalnum() or line_text[end] == "_"):
        return None
    start = end
    while start > 0 and (line_text[start - 1].isalnum() or line_text[start - 1] == "_"):
        start -= 1
    return line_text[start:end + 1]


def _param_info_for_keyword(script, line_1based, col, keyword_name):
    """Build hover info for a keyword argument by matching it in the enclosing
    function's signature."""
    signatures = script.get_signatures(line_1based, col)
    if not signatures:
        return None
    sig = signatures[0]
    for param in sig.params:
        if param.name == keyword_name:
            doc = param.description.strip() if param.description else ""
            if not doc:
                doc = sig.docstring() or ""
            return {
                "signature": f"{keyword_name}: parameter of {sig.name}",
                "doc": doc,
                "definition": sig.module_name or "",
            }
    return None


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
    # Collect definitions first (accurate types), then references
    # (Jedi may report less-accurate types for references — e.g. "instance"
    # for a class accessed via module attribute).  Keeping them separate
    # ensures definition types always win.
    try:
        def_names = script.get_names(all_scopes=True, definitions=True, references=False)
        ref_names = script.get_names(all_scopes=True, definitions=False, references=True)
    except TypeError:
        def_names = script.get_names(all_scopes=True)
        ref_names = []
    if not def_names and not ref_names:
        return []

    # 1. Collect (name_str, type) for each name, processing definitions
    #    first so their more-accurate types take priority.
    TYPE_PRIORITY = {
        "function": 6,
        "class": 5,
        "module": 4,
        "parameter": 3,
        "property": 2,
        "variable": 1,
    }
    name_type = {}  # name_str → (priority, type)
    for n in def_names:
        token_type = _jedi_type_to_string(n.type)
        if token_type == "keyword":
            continue
        prio = TYPE_PRIORITY.get(token_type, 0)
        old = name_type.get(n.name)
        if old is None or prio > old[0]:
            name_type[n.name] = (prio, token_type)

    # Fill in gaps with reference-only names (external names like
    # json.JSONDecodeError or __name__ that have no local definition).
    for n in ref_names:
        if n.name in name_type:
            continue  # definition type already known, keep it
        token_type = _jedi_type_to_string(n.type)
        if token_type == "keyword":
            continue
        # Jedi may report "instance" for references to classes accessed via
        # attribute (e.g. json.JSONDecodeError).  Use infer() at the
        # reference site to get the true type.
        if token_type in ("variable", "text"):
            try:
                inferred = script.infer(n.line, n.column)
                if inferred:
                    resolved = _jedi_type_to_string(inferred[0].type)
                    if resolved != "keyword":
                        token_type = resolved
            except Exception:
                pass
        prio = TYPE_PRIORITY.get(token_type, 0)
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
                # Both SMD ("code_b64") and standalone ("code") now send
                # base64-encoded code to avoid JSON/UTF-8 surrogate issues.
                raw = req.get("code_b64", "") or req.get("code", "")
                try:
                    code = base64.b64decode(raw).decode("utf-8")
                except Exception:
                    code = ""
                script = jedi.Script(code=code, path="untitled.py")
                if action == "complete":
                    data = handle_complete(script, line_1based, col)
                elif action == "hover":
                    data = handle_hover(script, code, line_1based, col)
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
