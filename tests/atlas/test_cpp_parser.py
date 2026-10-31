#!/usr/bin/env python3
"""C++ qualification tests for the ATLAS regex scanner.

The scanner is not an AST.  These tests pin the cases the audit found broken:
nested namespaces, out-of-class definitions, methods, free functions and
overloads.  Everything else stays explicitly "medium" confidence.
"""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT / "tools" / "atlas") not in sys.path:
    sys.path.insert(0, str(ROOT / "tools" / "atlas"))

from atlas_lib.indexer import parse_cpp  # noqa: E402


def symbols(text: str) -> list[dict]:
    return parse_cpp(text)["symbols"]


def named(items: list[dict], name: str) -> list[dict]:
    return [item for item in items if item["name"] == name]


class CppQualificationTest(unittest.TestCase):
    def test_out_of_class_definition_keeps_class_and_namespace(self) -> None:
        items = symbols(
            "namespace loblox {\n"
            "class Widget {\n"
            "public:\n"
            "    int value() const;\n"
            "    void setValue(int value);\n"
            "};\n"
            "int Widget::value() const { return 1; }\n"
            "void Widget::setValue(int value) { (void)value; }\n"
            "}\n"
        )
        value = named(items, "value")
        self.assertTrue(any(item["qualified"] == "loblox::Widget::value" for item in value), value)
        self.assertTrue(any(item["kind"] == "method" for item in value), value)
        set_value = named(items, "setValue")
        self.assertTrue(any(item["qualified"] == "loblox::Widget::setValue" for item in set_value))
        self.assertTrue(any(item["kind"] == "method" for item in set_value))

    def test_nested_namespace_is_accumulated(self) -> None:
        items = symbols(
            "namespace loblox {\n"
            "namespace editor {\n"
            "void refresh() { }\n"
            "}\n"
            "}\n"
        )
        refresh = named(items, "refresh")
        self.assertEqual(refresh[0]["qualified"], "loblox::editor::refresh", refresh)
        self.assertEqual(refresh[0]["kind"], "function")

    def test_compact_nested_namespace_declaration(self) -> None:
        items = symbols("namespace loblox::editor {\nvoid refreshAll() { }\n}\n")
        refresh = named(items, "refreshAll")
        self.assertEqual(refresh[0]["qualified"], "loblox::editor::refreshAll", refresh)

    def test_already_qualified_definition_is_not_prefixed_twice(self) -> None:
        items = symbols(
            "namespace loblox {\n"
            "class Widget { public: int value() const; };\n"
            "int loblox::Widget::value() const { return 2; }\n"
            "}\n"
        )
        qualified = [item["qualified"] for item in named(items, "value")]
        self.assertIn("loblox::Widget::value", qualified)
        self.assertTrue(all(not item.startswith("loblox::loblox::") for item in qualified), qualified)

    def test_free_function_is_not_a_method(self) -> None:
        items = symbols("namespace loblox {\nint computeTotal(int a) { return a; }\n}\n")
        compute = named(items, "computeTotal")
        self.assertEqual(compute[0]["qualified"], "loblox::computeTotal", compute)
        self.assertEqual(compute[0]["kind"], "function")

    def test_overloads_keep_both_definitions(self) -> None:
        items = symbols(
            "namespace loblox {\n"
            "int total(int a) { return a; }\n"
            "int total(int a, int b) { return a + b; }\n"
            "}\n"
        )
        totals = named(items, "total")
        self.assertEqual(len(totals), 2, totals)
        self.assertEqual({item["qualified"] for item in totals}, {"loblox::total"})
        self.assertNotEqual(totals[0]["line"], totals[1]["line"])

    def test_inline_method_uses_owner_class(self) -> None:
        items = symbols(
            "namespace loblox {\n"
            "class Widget {\n"
            "public:\n"
            "    int value() const { return 3; }\n"
            "};\n"
            "}\n"
        )
        value = named(items, "value")
        self.assertTrue(value)
        self.assertEqual(value[0]["qualified"], "loblox::Widget::value", value)
        self.assertEqual(value[0]["kind"], "method")

    def test_known_limitation_inline_namespace_body_is_skipped(self) -> None:
        # Documented limitation: the scanner is line-oriented and skips lines
        # that start with ``namespace``, so a whole namespace written on one
        # line produces no symbols.  The repository never writes C++ this way.
        items = symbols("namespace loblox { void inlineBody() { } }\n")
        self.assertEqual(named(items, "inlineBody"), [])

    def test_global_function_stays_global(self) -> None:
        items = symbols("int main() { return 0; }\n")
        main = named(items, "main")
        self.assertEqual(main[0]["qualified"], "main")
        self.assertEqual(main[0]["kind"], "function")


if __name__ == "__main__":
    unittest.main(verbosity=2)
