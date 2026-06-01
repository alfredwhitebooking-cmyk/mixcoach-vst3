#!/usr/bin/env python3
"""
ast_parser.py — C++ Structural Parser v1.0

AST PARSING SYSTEM (requirement #4):
Parses C++ source code structurally to extract:
  - Classes (with inheritance, base classes)
  - Methods (with parameters, return types, const, override, virtual)
  - Namespaces
  - Includes (with classification: local, JUCE, standard, system)
  - Enums (with values)
  - Callbacks (std::function, juce::Listener)
  - Forward declarations
  - Templates
  - Structs (POD types)

Usage:
    python ast_parser.py --scan                         # Scan entire project
    python ast_parser.py --file Source/Common/Types.h    # Parse single file
    python ast_parser.py --update                        # Update SYMBOL_GRAPH.json
    python ast_parser.py --query "class name"            # Find symbol in graph

Features:
    - No external dependencies (pure Python regex-based)
    - Handles JUCE-specific patterns (JUCE_DECLARE_NON_COPYABLE, etc.)
    - Extracts relationships (inherits, contains, calls, callback_of)
    - Generates SYMBOL_GRAPH.json compatible with other tools
"""

import json
import re
import sys
import os
from pathlib import Path
from datetime import datetime
from collections import defaultdict

# ─── Paths ─────────────────────────────────────────────────────────────────
PROJECT_ROOT = Path(__file__).parent.parent.resolve()

# Fallback: if still wrong, use CWD
if not (PROJECT_ROOT / "Source").exists():
    PROJECT_ROOT = Path.cwd().resolve()
SOURCE_DIR = PROJECT_ROOT / "Source"
SYMBOL_GRAPH_PATH = PROJECT_ROOT / "SYMBOL_GRAPH.json"
PROJECT_INDEX_PATH = PROJECT_ROOT / "PROJECT_INDEX.json"

# ─── Include Classification ───────────────────────────────────────────────
JUCE_MODULES = {
    "juce_audio_basics", "juce_audio_devices", "juce_audio_formats",
    "juce_audio_processors", "juce_audio_plugin_client", "juce_audio_utils",
    "juce_core", "juce_data_structures", "juce_dsp", "juce_events",
    "juce_graphics", "juce_gui_basics", "juce_gui_extra",
    "juce_opengl", "juce_osc", "juce_video",
}

STD_HEADERS = {
    "array", "vector", "deque", "list", "map", "set", "unordered_map",
    "unordered_set", "string", "string_view", "memory", "functional",
    "algorithm", "cmath", "cstdint", "cstring", "cstddef", "cassert",
    "atomic", "mutex", "thread", "chrono", "fstream", "sstream",
    "iostream", "optional", "variant", "tuple", "any", "type_traits",
    "utility", "limits", "numeric", "iterator", "ranges", "span",
    "format", "filesystem", "exception", "stdexcept", "initializer_list",
}


class CppParser:
    """Parses C++ source files and extracts structural information."""

    def __init__(self):
        # Results
        self.includes = []          # list of IncludeInfo
        self.namespaces = []        # list of namespaces
        self.classes = []           # list of ClassInfo
        self.structs = []           # list of ClassInfo (POD)
        self.enums = []             # list of EnumInfo
        self.functions = []         # list of free functions
        self.forward_decls = []     # list of forward declarations
        self.callbacks = []         # list of callback types
        self.typedefs = []          # list of typedef/using aliases
        self.templates = []         # list of template declarations
        self.macros = []            # list of #define macros

        # Current context (for tracking nesting)
        self.current_namespace = ""
        self.current_class = ""
        self.line_number = 0

    def parse_file(self, file_path):
        """Parse a single C++ file and return structured data."""
        file_path = Path(file_path)
        self._reset()
        self.file_path = str(file_path)

        try:
            with open(file_path, "r", encoding="utf-8") as f:
                content = f.read()
        except Exception as e:
            return {"error": str(e), "file": str(file_path)}

        # Normalize line endings
        content = content.replace("\r\n", "\n")

        # Remove block comments (preserve line numbers approximately)
        clean_content = self._remove_comments(content)

        lines = clean_content.split("\n")

        # Multi-line state tracking
        in_template = False
        template_buffer = ""
        in_pragma_once = False
        in_include = False
        include_buffer = ""
        in_multi_line_decl = False
        decl_buffer = ""

        for i, line in enumerate(lines):
            self.line_number = i + 1
            stripped = line.strip()

            if not stripped or stripped.startswith("//"):
                continue

            # Handle multi-line declarations
            if in_multi_line_decl:
                decl_buffer += " " + stripped
                if ";" in stripped or "{" in stripped or "}" in stripped:
                    in_multi_line_decl = False
                    self._parse_declaration(decl_buffer)
                    decl_buffer = ""
                continue

            if ";" not in stripped and "{" not in stripped and "}" not in stripped and not stripped.endswith("\\"):
                # Could be multi-line start
                if self._is_declaration_start(stripped):
                    in_multi_line_decl = True
                    decl_buffer = stripped
                    continue

            # ─── Pragma ───────────────────────────────────────────────
            if "#pragma once" in stripped:
                in_pragma_once = True
                continue

            # ─── Include ──────────────────────────────────────────────
            include_match = re.match(r'#include\s+["<](.+?)[">]', stripped)
            if include_match:
                self._parse_include(include_match.group(1), stripped)
                continue

            # ─── Define ───────────────────────────────────────────────
            define_match = re.match(r'#define\s+(\w+)(?:\s+(.*))?', stripped)
            if define_match:
                self.macros.append({
                    "name": define_match.group(1),
                    "value": (define_match.group(2) or "").strip(),
                    "line": self.line_number,
                })
                continue

            # ─── Forward declaration ──────────────────────────────────
            fwd_match = re.match(r'(?:class|struct)\s+(\w+)\s*;', stripped)
            if fwd_match:
                self.forward_decls.append({
                    "name": fwd_match.group(1),
                    "kind": "class" if "class" in stripped[:10] else "struct",
                    "line": self.line_number,
                    "namespace": self.current_namespace,
                })
                continue

            # ─── Namespace ────────────────────────────────────────────
            ns_match = re.match(r'namespace\s+(\w+)\s*\{', stripped)
            if ns_match:
                self.current_namespace = ns_match.group(1)
                self.namespaces.append(ns_match.group(1))
                continue

            ns_close = re.match(r'\}\s*//\s*namespace', stripped) or stripped == "}"
            if ns_close and self.current_namespace:
                self.current_namespace = ""
                continue

            # ─── Enum ─────────────────────────────────────────────────
            enum_match = re.match(r'enum\s+(?:class\s+)?(\w+)\s*(?::\s*(\w+))?\s*\{', stripped)
            if enum_match:
                self._parse_enum(lines, i, enum_match)
                continue

            # ─── Template ─────────────────────────────────────────────
            if stripped.startswith("template<") or stripped.startswith("template <"):
                template_buffer = stripped
                if ">" not in stripped:
                    in_template = True
                continue

            if in_template:
                template_buffer += " " + stripped
                if ">" in stripped:
                    in_template = False
                    stripped = template_buffer + " " + stripped
                    template_buffer = ""

            # ─── Class / Struct ───────────────────────────────────────
            class_match = re.match(
                r'(?:template\s*<[^>]+>\s*)?(?:class|struct)\s+(\w+)'
                r'(?:\s*:\s*(?:public|private|protected)\s+([^{]+))?'
                r'\s*\{',
                stripped
            )
            if class_match:
                is_struct = "struct" in stripped[:15]
                class_info = self._parse_class(lines, i, class_match, is_struct)
                if is_struct:
                    self.structs.append(class_info)
                else:
                    self.classes.append(class_info)
                continue

            # ─── Free function (outside class) ────────────────────────
            func_match = re.match(
                r'(?:static\s+)?(?:inline\s+)?'
                r'(?:const\s+)?(?:\w+::)*(\w+)\s+'
                r'(\w+)\s*\(([^)]*)\)\s*'
                r'(?:const\s*)?(?:override\s*)?(?:=\s*0\s*)?[;{]',
                stripped
            )
            if func_match and self.current_class == "":
                self.functions.append({
                    "return_type": func_match.group(1),
                    "name": func_match.group(2),
                    "params": func_match.group(3),
                    "line": self.line_number,
                    "namespace": self.current_namespace,
                })
                continue

            # ─── Typedef / Using ─────────────────────────────────────
            using_match = re.match(r'using\s+(\w+)\s*=\s*(.+);', stripped)
            if using_match:
                self.typedefs.append({
                    "alias": using_match.group(1),
                    "type": using_match.group(2).strip(),
                    "line": self.line_number,
                    "namespace": self.current_namespace,
                })
                # Check if it's a callback
                if "std::function" in using_match.group(2) or "juce::Listener" in using_match.group(2):
                    self.callbacks.append({
                        "name": using_match.group(1),
                        "signature": using_match.group(2).strip(),
                        "line": self.line_number,
                        "namespace": self.current_namespace,
                    })
                continue

        return self._to_dict()

    def _reset(self):
        """Reset all parsed data for a new file."""
        self.includes.clear()
        self.namespaces.clear()
        self.classes.clear()
        self.structs.clear()
        self.enums.clear()
        self.functions.clear()
        self.forward_decls.clear()
        self.callbacks.clear()
        self.typedefs.clear()
        self.templates.clear()
        self.macros.clear()
        self.current_namespace = ""
        self.current_class = ""
        self.file_path = ""

    def _remove_comments(self, content):
        """Remove block comments (/* ... */) from content."""
        # Remove block comments
        result = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
        return result

    def _parse_include(self, include_path, original_line):
        """Classify and store an include."""
        is_angle = "<" in original_line
        if is_angle:
            if include_path.startswith("juce_"):
                kind = "juce"
                module = include_path
            elif include_path in STD_HEADERS or include_path.startswith("c"):
                kind = "standard"
                module = "std"
            else:
                kind = "system"
                module = include_path
        else:
            kind = "local"
            module = "project"
            # Normalize path separators
            include_path = include_path.replace("\\", "/")

        self.includes.append({
            "path": include_path,
            "kind": kind,
            "angle": is_angle,
            "line": self.line_number,
        })

    def _is_declaration_start(self, line):
        """Check if line might start a multi-line declaration."""
        keywords = ["static", "const", "void", "int", "float", "double",
                     "bool", "char", "auto", "juce::", "std::", "&", "*"]
        return any(kw in line for kw in keywords) and "(" in line and ")" not in line

    def _parse_declaration(self, decl):
        """Parse a multi-line declaration."""
        # Try to extract method declaration
        func_match = re.match(
            r'(?:(virtual|static|inline)\s+)?'
            r'(?:const\s+)?(\w+(?:::)*\w*(?:<[^>]*>)?)\s+'
            r'(\w+)\s*\(([^)]*)\)\s*'
            r'(?:const\s*)?(?:override\s*)?(?:=\s*0\s*)?[;{]',
            decl
        )
        if func_match:
            method = {
                "return_type": func_match.group(2),
                "name": func_match.group(3),
                "params": func_match.group(4),
                "qualifiers": ("virtual" if "virtual" in decl else ""),
                "line": self.line_number,
            }
            if self.current_class:
                for cls in self.classes:
                    if cls["name"] == self.current_class:
                        cls["methods"].append(method)
                        break
            else:
                self.functions.append(method)

    def _parse_enum(self, lines, start_index, match):
        """Parse an enum definition spanning one or more lines.

        Handles both:
          enum class Foo { A, B, C };                    // single-line
          enum class Foo {
              A, B, C
          };                                            // multi-line
        """
        enum_name = match.group(1)
        enum_type = match.group(2) if match.group(2) else None
        values = []

        # Extract values from the SAME line as the opening '{'
        first_line = lines[start_index].strip()
        brace_idx = first_line.index("{")
        rest_of_line = first_line[brace_idx + 1:].strip()

        # Strip trailing ';' and '}'
        rest_of_line = rest_of_line.rstrip(";").rstrip("}").strip()

        if rest_of_line:
            # Values are on the same line as the opening brace
            for part in rest_of_line.split(","):
                part = part.strip()
                if part and not part.startswith("//"):
                    val_match = re.match(r'(\w+)\s*(?:=\s*([^,]+))?', part)
                    if val_match:
                        values.append({
                            "name": val_match.group(1),
                            "value": (val_match.group(2) or "").strip(),
                        })

        if not rest_of_line:
            # Multi-line: extract values from subsequent lines
            brace_count = 0
            i = start_index

            while i < len(lines):
                line = lines[i].strip()

                brace_count += line.count("{") - line.count("}")

                # Exit when brace_count reaches 0 (matching brace closed)
                if brace_count <= 0:
                    break

                if i > start_index:
                    # Extract enum values
                    val_match = re.match(r'(\w+)\s*(?:=\s*([^,}]+))?', line)
                    if val_match:
                        values.append({
                            "name": val_match.group(1),
                            "value": (val_match.group(2) or "").strip(),
                        })
                i += 1

        self.enums.append({
            "name": enum_name,
            "type": enum_type or "int",
            "values": values,
            "scoped": "class" in lines[start_index],
            "line": start_index + 1,
            "namespace": self.current_namespace,
        })

    def _parse_class(self, lines, start_index, match, is_struct=False):
        """Parse a class/struct definition spanning multiple lines."""
        class_name = match.group(1)
        base_classes_str = match.group(2) or ""
        base_classes = []

        # Parse base classes
        if base_classes_str:
            for bc in re.split(r'\s*,\s*', base_classes_str):
                bc_match = re.match(r'(?:public|private|protected)\s+(.+)$', bc.strip())
                if bc_match:
                    # Handle templates: SomeClass<T>
                    base_name = bc_match.group(1).split("<")[0].strip()
                    base_classes.append(base_name)

        class_info = {
            "name": class_name,
            "kind": "struct" if is_struct else "class",
            "namespace": self.current_namespace,
            "base_classes": base_classes,
            "methods": [],
            "members": [],
            "line": start_index + 1,
            "inner_classes": [],
            "is_abstract": False,
        }

        # Track current class for nested declarations
        saved_current_class = self.current_class
        self.current_class = class_name

        # Parse body
        body_lines = []
        brace_count = 1
        i = start_index + 1
        while i < len(lines) and brace_count > 0:
            line = lines[i].strip()
            brace_count += line.count("{") - line.count("}")
            if brace_count > 0:
                body_lines.append(line)

            # Check for pure virtual (abstract)
            if "= 0" in line or "=0" in line:
                class_info["is_abstract"] = True

            # Extract methods
            func_match = re.match(
                r'(?:(virtual|static|inline)\s+)?'
                r'(?:const\s+)?(\w+(?:::)*\w*(?:<[^>]*>)?)\s+'
                r'(\w+)\s*\(([^)]*)\)\s*'
                r'(?:const\s*)?(?:override\s*)?(?:=\s*0\s*)?[;{]',
                line
            )
            if func_match:
                method = {
                    "return_type": func_match.group(2),
                    "name": func_match.group(3),
                    "params": func_match.group(4),
                    "qualifiers": func_match.group(1) or "",
                    "override": "override" in line,
                    "virtual": "virtual" in line or "virtual" in class_info.get("base_classes", []),
                    "pure_virtual": "= 0" in line or "=0" in line,
                    "const": "const" in line and not line.strip().startswith("const"),
                    "line": start_index + 1 + (i - start_index),
                    "class": class_name,
                }
                class_info["methods"].append(method)
            elif "JUCE_DECLARE_NON_COPYABLE" in line or "JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR" in line:
                class_info["members"].append({
                    "name": line.strip().rstrip(";"),
                    "kind": "juce_macro",
                    "line": start_index + 1 + (i - start_index),
                })

            i += 1

        # Restore previous class context
        self.current_class = saved_current_class

        return class_info

    def _to_dict(self):
        """Convert parsed data to a JSON-serializable dict."""
        return {
            "file": self.file_path,
            "includes": self.includes,
            "namespaces": self.namespaces,
            "classes": self.classes,
            "structs": self.structs,
            "enums": self.enums,
            "functions": self.functions,
            "forward_decls": self.forward_decls,
            "callbacks": self.callbacks,
            "typedefs": self.typedefs,
            "macros": self.macros,
            "line_count": self.line_number,
        }


# ═══════════════════════════════════════════════════════════════════════════
#  SymbolGraphGenerator — Builds SYMBOL_GRAPH.json from parsed files
# ═══════════════════════════════════════════════════════════════════════════
class SymbolGraphGenerator:
    """Aggregates parsed data from multiple files into a unified symbol graph."""

    def __init__(self):
        self.symbols = {}          # symbol_name -> SymbolInfo
        self.by_file = {}          # file_path -> [symbol_names]
        self.by_namespace = {}     # namespace -> [symbol_names]
        self.relationships = []    # list of relationships
        self.include_graph = {}    # file_path -> [included_files]

    def add_file_parsed(self, parsed):
        """Add a parsed file to the graph."""
        file_path = parsed.get("file", "")
        if not file_path:
            return

        if file_path not in self.include_graph:
            self.include_graph[file_path] = []
        for inc in parsed.get("includes", []):
            self.include_graph[file_path].append(inc["path"])

        file_symbols = []

        # Add enums
        for enum in parsed.get("enums", []):
            sym_name = enum["name"]
            ns = enum.get("namespace", "")
            full_name = f"{ns}::{sym_name}" if ns else sym_name
            self.symbols[full_name] = {
                "type": "enum",
                "name": sym_name,
                "full_name": full_name,
                "namespace": ns,
                "file": file_path,
                "line": enum.get("line", 0),
                "values": [v["name"] for v in enum.get("values", [])],
                "scoped": enum.get("scoped", False),
                "referenced_by": [],
                "references": [],
            }
            file_symbols.append(full_name)

        # Add classes
        for cls in parsed.get("classes", []) + parsed.get("structs", []):
            sym_name = cls["name"]
            ns = cls.get("namespace", "")
            full_name = f"{ns}::{sym_name}" if ns else sym_name
            methods_list = []
            for m in cls.get("methods", []):
                method_key = f"{full_name}::{m['name']}"
                methods_list.append(method_key)
                self.symbols[method_key] = {
                    "type": "method",
                    "name": m["name"],
                    "full_name": method_key,
                    "class": full_name,
                    "namespace": ns,
                    "file": file_path,
                    "line": m.get("line", 0),
                    "return_type": m.get("return_type", ""),
                    "params": m.get("params", ""),
                    "qualifiers": m.get("qualifiers", ""),
                    "override": m.get("override", False),
                    "virtual": m.get("virtual", False),
                    "pure_virtual": m.get("pure_virtual", False),
                    "const": m.get("const", False),
                    "referenced_by": [],
                    "references": [],
                }

            self.symbols[full_name] = {
                "type": cls["kind"],
                "name": sym_name,
                "full_name": full_name,
                "namespace": ns,
                "file": file_path,
                "line": cls.get("line", 0),
                "base_classes": cls.get("base_classes", []),
                "methods": methods_list,
                "is_abstract": cls.get("is_abstract", False),
                "referenced_by": [],
                "references": [],
            }
            file_symbols.append(full_name)

            # Add inheritance relationships
            for base in cls.get("base_classes", []):
                self.relationships.append({
                    "type": "inherits",
                    "from": base,
                    "to": full_name,
                    "file": file_path,
                })

        # Add callbacks
        for cb in parsed.get("callbacks", []):
            sym_name = cb["name"]
            ns = cb.get("namespace", "")
            full_name = f"{ns}::{sym_name}" if ns else sym_name
            self.symbols[full_name] = {
                "type": "callback",
                "name": sym_name,
                "full_name": full_name,
                "namespace": ns,
                "file": file_path,
                "line": cb.get("line", 0),
                "signature": cb.get("signature", ""),
                "referenced_by": [],
                "references": [],
            }
            file_symbols.append(full_name)

        # Add forward declarations as symbols
        for fwd in parsed.get("forward_decls", []):
            sym_name = fwd["name"]
            ns = fwd.get("namespace", "")
            full_name = f"{ns}::{sym_name}" if ns else sym_name
            if full_name not in self.symbols:
                self.symbols[full_name] = {
                    "type": "forward_decl",
                    "name": sym_name,
                    "full_name": full_name,
                    "namespace": ns,
                    "file": file_path,
                    "line": fwd.get("line", 0),
                    "kind": fwd.get("kind", "class"),
                    "referenced_by": [],
                    "references": [],
                }
                file_symbols.append(full_name)

        # Update by_file and by_namespace
        self.by_file[file_path] = file_symbols
        for sym_name in file_symbols:
            sym = self.symbols.get(sym_name, {})
            ns = sym.get("namespace", "")
            if ns:
                self.by_namespace.setdefault(ns, []).append(sym_name)

    def build_relationships(self):
        """Build cross-reference relationships between symbols.

        Strategy:
        1. Inheritance relationships (from parsing): already in self.relationships
        2. Content-scan: for each file, scan its source text for known symbol names
           defined in other files. Only creates a relationship when the symbol name
           actually appears as a word boundary in the source text.
        3. Include graph (file-level): preserved in self.include_graph.
        """
        # Read all file contents once
        file_contents = {}
        for file_path in self.by_file:
            try:
                fp = Path(file_path)
                if fp.exists():
                    with open(fp, "r", encoding="utf-8") as f:
                        file_contents[file_path] = f.read()
            except Exception:
                pass

        # Cache: symbol_name -> list of (file_path, symbol_fullname) for quick lookup
        # We need to find which symbols are defined in which files
        symbol_defined_in = {}  # short_name -> list of (full_name, file_path)
        for sym_name, sym in self.symbols.items():
            short = sym.get("name", "")
            if short and len(short) >= 2:
                sym_file = sym.get("file", "")
                symbol_defined_in.setdefault(short, []).append((sym_name, sym_file))

        # For each file, scan its source for symbol names from OTHER files
        for file_path, file_symbols in self.by_file.items():
            content = file_contents.get(file_path, "")
            if not content:
                continue

            for sym_short, sym_defs in symbol_defined_in.items():
                # Only check if this symbol name actually appears in the source
                if sym_short not in content:
                    continue

                for sym_name, sym_file in sym_defs:
                    # Skip self-references (symbol defined in same file)
                    if sym_file == file_path:
                        continue

                    # Create relationship: all symbols in this file reference the found symbol
                    for file_sym in file_symbols:
                        if file_sym not in self.symbols or sym_name not in self.symbols:
                            continue

                        # Add reference
                        refs = self.symbols[file_sym].setdefault("references", [])
                        if sym_name not in refs:
                            refs.append(sym_name)

                        # Add referenced_by
                        refd_by = self.symbols[sym_name].setdefault("referenced_by", [])
                        if file_sym not in refd_by:
                            refd_by.append(file_sym)

                        # Record as a relationship
                        self.relationships.append({
                            "type": "references",
                            "from": file_sym,
                            "to": sym_name,
                            "file": file_path,
                        })

    def get_symbol(self, name):
        """Find symbol by name (partial match supported)."""
        if name in self.symbols:
            return self.symbols[name]
        for sym_name, sym in self.symbols.items():
            if name in sym_name:
                return sym
        return None

    def find_methods(self, class_name):
        """Find all methods of a class."""
        result = []
        for sym_name, sym in self.symbols.items():
            if sym.get("type") == "method" and sym.get("class") == class_name:
                result.append(sym)
        return result

    def find_references(self, symbol_name):
        """Find all symbols that reference a given symbol."""
        sym = self.symbols.get(symbol_name, {})
        return sym.get("referenced_by", [])

    def to_dict(self):
        """Serialize graph to dictionary."""
        return {
            "version": "2.0.0",
            "generated": datetime.now().isoformat(),
            "description": "Symbol relationship graph for MixCoach JUCE project. "
                           "Generated by ast_parser.py from C++ source analysis.",
            "total_symbols": len(self.symbols),
            "total_relationships": len(self.relationships),
            "symbols": self.symbols,
            "relationships": self.relationships,
            "include_graph": self.include_graph,
            "by_file": self.by_file,
            "by_namespace": self.by_namespace,
        }

    def save(self, path=None):
        """Save graph to JSON file."""
        path = path or SYMBOL_GRAPH_PATH
        with open(path, "w", encoding="utf-8") as f:
            json.dump(self.to_dict(), f, indent=2, ensure_ascii=False)
        print(f"[SYMBOL_GRAPH] Saved: {path}")
        print(f"  Symbols: {len(self.symbols)}")
        print(f"  Relationships: {len(self.relationships)}")
        print(f"  Files: {len(self.by_file)}")


# ═══════════════════════════════════════════════════════════════════════════
#  Scanning Logic
# ═══════════════════════════════════════════════════════════════════════════
def scan_project():
    """Scan entire Source directory and build symbol graph."""
    parser = CppParser()
    generator = SymbolGraphGenerator()
    total_files = 0
    errors = []

    # Get source files from PROJECT_INDEX.json for priority ordering
    index_files = set()
    try:
        with open(PROJECT_INDEX_PATH, "r", encoding="utf-8") as f:
            index = json.load(f)
        for module in index.get("modules", []):
            for file_entry in module.get("files", []):
                path = file_entry.get("path", "")
                if path:
                    index_files.add(path)
    except (FileNotFoundError, json.JSONDecodeError):
        pass

    # Scan Source directory
    source_dir = SOURCE_DIR
    if source_dir.exists():
        for ext in ("*.h", "*.cpp"):
            for file_path in sorted(source_dir.rglob(ext)):
                relative = str(file_path.relative_to(PROJECT_ROOT)).replace("\\", "/")
                try:
                    parsed = parser.parse_file(file_path)
                    if "error" not in parsed:
                        generator.add_file_parsed(parsed)
                        total_files += 1
                    else:
                        errors.append((relative, parsed["error"]))
                except Exception as e:
                    errors.append((relative, str(e)))

    # Build cross-references
    generator.build_relationships()

    print(f"\n[SCAN] Complete: {total_files} files parsed")
    if errors:
        for f, err in errors[:5]:
            print(f"  [WARN] {f}: {err}")

    return generator


def parse_single_file(file_path):
    """Parse a single file and print results."""
    parser = CppParser()
    full_path = PROJECT_ROOT / file_path
    if not full_path.exists():
        full_path = Path(file_path)
    if not full_path.exists():
        print(f"[ERROR] File not found: {file_path}")
        return

    parsed = parser.parse_file(full_path)
    if "error" in parsed:
        print(f"[ERROR] {parsed['error']}")
        return

    print(f"\n{'='*60}")
    print(f"  File: {parsed['file']}")
    print(f"{'='*60}")
    print(f"  Lines: {parsed['line_count']}")
    print(f"  Includes: {len(parsed['includes'])}")
    print(f"  Namespaces: {parsed['namespaces']}")
    print(f"  Classes: {[c['name'] for c in parsed['classes']]}")
    print(f"  Structs: {[s['name'] for s in parsed['structs']]}")
    print(f"  Enums: {[e['name'] for e in parsed['enums']]}")
    print(f"  Free Functions: {[f['name'] for f in parsed['functions']]}")
    print(f"  Callbacks: {[c['name'] for c in parsed['callbacks']]}")
    print(f"  Forward Decls: {[f['name'] for f in parsed['forward_decls']]}")

    for cls in parsed['classes']:
        print(f"\n  Class: {cls['name']}")
        if cls['base_classes']:
            print(f"    Inherits: {cls['base_classes']}")
        if cls['is_abstract']:
            print(f"    [ABSTRACT]")
        for m in cls['methods']:
            qual = "V" if m.get("virtual") else " "
            ovr = "O" if m.get("override") else " "
            pur = "P" if m.get("pure_virtual") else " "
            print(f"    [{qual}{ovr}{pur}] {m['return_type']} {m['name']}({m['params']})")

    return parsed


def main():
    import argparse

    parser = argparse.ArgumentParser(description="C++ Structural Parser for MixCoach")
    parser.add_argument("--scan", action="store_true", help="Scan entire project and build symbol graph")
    parser.add_argument("--file", metavar="FILE", help="Parse a single C++ source file")
    parser.add_argument("--update", action="store_true", help="Update SYMBOL_GRAPH.json")
    parser.add_argument("--query", metavar="SYMBOL", help="Query symbol in existing graph")
    args = parser.parse_args()

    if not any(vars(args).values()):
        parser.print_help()
        sys.exit(1)

    if args.scan or args.update:
        generator = scan_project()
        if args.update or args.scan:
            generator.save()

    if args.file:
        parse_single_file(args.file)

    if args.query:
        try:
            with open(SYMBOL_GRAPH_PATH, "r", encoding="utf-8") as f:
                graph = json.load(f)
        except (FileNotFoundError, json.JSONDecodeError):
            print(f"[ERROR] No SYMBOL_GRAPH.json found. Run --scan first.")
            sys.exit(1)

        symbols = graph.get("symbols", {})
        query = args.query.lower()
        results = []
        for sym_name, sym in symbols.items():
            if query in sym_name.lower():
                results.append(sym)

        results.sort(key=lambda x: x.get("type", ""))
        print(f"\n[{len(results)}] symbols matching '{args.query}':")
        print("-" * 60)
        for r in results[:20]:
            print(f"  [{r.get('type', '?'):12s}] {r.get('full_name', r.get('name', '?'))}")
            print(f"         File: {r.get('file', '?'):40s} Line: {r.get('line', '?')}")
            if r.get("base_classes"):
                print(f"         Inherits: {', '.join(r['base_classes'])}")
            if r.get("methods"):
                print(f"         Methods: {len(r['methods'])}")
            if r.get("referenced_by"):
                print(f"         Referenced by: {len(r['referenced_by'])} symbols")

        if len(results) > 20:
            print(f"  ... and {len(results) - 20} more")


if __name__ == "__main__":
    main()
