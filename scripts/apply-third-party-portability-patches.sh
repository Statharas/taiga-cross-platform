#!/usr/bin/env bash
set -euo pipefail

python3 - <<'PY'
from pathlib import Path

def read(path):
    return path.read_text(encoding="utf-8")

def write(path, text):
    path.write_text(text, encoding="utf-8")

anitomy = Path("deps/anitomy/CMakeLists.txt")
text = read(anitomy)
old = """add_subdirectory(include)
add_subdirectory(src)
add_subdirectory(test)

enable_testing()
add_test(NAME "Unit" COMMAND anitomy-tests)
add_test(NAME "Data" COMMAND anitomy-tests --test-data WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test)
"""
new = """add_subdirectory(include)

if (PROJECT_IS_TOP_LEVEL)
\tadd_subdirectory(src)
\tadd_subdirectory(test)

\tenable_testing()
\tadd_test(NAME "Unit" COMMAND anitomy-tests)
\tadd_test(NAME "Data" COMMAND anitomy-tests --test-data WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test)
endif()
"""
if old in text:
    write(anitomy, text.replace(old, new))

monolog = Path("deps/monolog/src/monolog.cpp")
text = read(monolog)
old = """  std::tm tm = {};
  localtime_s(&tm, &now);
"""
new = """  std::tm tm = {};
#ifdef _WIN32
  localtime_s(&tm, &now);
#else
  localtime_r(&now, &tm);
#endif
"""
if old in text:
    write(monolog, text.replace(old, new))

keyword = Path("deps/anitomy/include/anitomy/detail/keyword.hpp")
text = read(keyword)
if "#include <iterator>" not in text:
    text = text.replace("#include <algorithm>\n", "#include <algorithm>\n#include <iterator>\n")
old = """  [[nodiscard]] size_t operator()(const std::string& view) const noexcept {
    auto str = view | std::views::transform(to_lower<char>) | std::ranges::to<std::string>();
    return std::hash<std::string>()(str);
  }
"""
new = """  [[nodiscard]] size_t operator()(const std::string& view) const noexcept {
    std::string str;
    str.reserve(view.size());
    std::ranges::transform(view, std::back_inserter(str), to_lower<char>);
    return std::hash<std::string>()(str);
  }
"""
if old in text:
    write(keyword, text.replace(old, new))

element = Path("deps/anitomy/include/anitomy/detail/element.hpp")
text = read(element)
old = """  const auto delimiters = tokens | std::views::filter(is_delimiter_token) |
                          std::views::transform(first_code_point) |
                          std::ranges::to<std::set<char32_t>>();
"""
new = """  std::set<char32_t> delimiters;
  for (const auto& token : tokens) {
    if (is_delimiter_token(token)) delimiters.insert(first_code_point(token));
  }
"""
if old in text:
    write(element, text.replace(old, new))

parser = Path("deps/anitomy/include/anitomy/detail/parser.hpp")
text = read(parser)
old = """  [[nodiscard]] constexpr auto&& elements(this auto&& self) noexcept {
    return std::forward<decltype(self)>(self).elements_;
  }

  [[nodiscard]] constexpr auto&& tokens(this auto&& self) noexcept {
    return std::forward<decltype(self)>(self).tokens_;
  }
"""
new = """  [[nodiscard]] constexpr auto& elements() & noexcept {
    return elements_;
  }

  [[nodiscard]] constexpr const auto& elements() const& noexcept {
    return elements_;
  }

  [[nodiscard]] constexpr auto&& elements() && noexcept {
    return std::move(elements_);
  }

  [[nodiscard]] constexpr auto& tokens() & noexcept {
    return tokens_;
  }

  [[nodiscard]] constexpr const auto& tokens() const& noexcept {
    return tokens_;
  }

  [[nodiscard]] constexpr auto&& tokens() && noexcept {
    return std::move(tokens_);
  }
"""
if old in text:
    write(parser, text.replace(old, new))

tokenizer = Path("deps/anitomy/include/anitomy/detail/tokenizer.hpp")
text = read(tokenizer)
old = """  [[nodiscard]] constexpr auto&& tokens(this auto&& self) noexcept {
    return std::forward<decltype(self)>(self).tokens_;
  }
"""
new = """  [[nodiscard]] constexpr auto& tokens() & noexcept {
    return tokens_;
  }

  [[nodiscard]] constexpr const auto& tokens() const& noexcept {
    return tokens_;
  }

  [[nodiscard]] constexpr auto&& tokens() && noexcept {
    return std::move(tokens_);
  }
"""
if old in text:
    text = text.replace(old, new)
old = """      const auto starts_with = [&prefix](std::string_view keyword) {
        return std::ranges::starts_with(keyword, prefix, equal_to);
      };
      return std::ranges::find_if(keys, starts_with) != keys.end();
"""
new = """      const auto starts_with = [&prefix](std::string_view keyword) {
        return std::ranges::equal(prefix, keyword | std::views::take(prefix.size()), equal_to);
      };
      return std::ranges::find_if(keys.begin(), keys.end(), starts_with) != keys.end();
"""
if old in text:
    text = text.replace(old, new)
write(tokenizer, text)

cli = Path("deps/anitomy/include/anitomy/detail/cli.hpp")
text = read(cli)
old = """  [[nodiscard]] static inline args_t parse_args(int argc, char* argv[]) noexcept {
    return std::span{argv, static_cast<size_t>(argc)} | std::ranges::to<args_t>();
  }
"""
new = """  [[nodiscard]] static inline args_t parse_args(int argc, char* argv[]) noexcept {
    args_t args;
    args.reserve(static_cast<size_t>(argc));
    for (size_t i = 0; i < static_cast<size_t>(argc); ++i) args.emplace_back(argv[i]);
    return args;
  }
"""
if old in text:
    write(cli, text.replace(old, new))

cli_table = Path("deps/anitomy/include/anitomy/detail/cli/table.hpp")
text = read(cli_table)
if "#include <iterator>" not in text:
    text = text.replace("#include <algorithm>\n", "#include <algorithm>\n#include <iterator>\n")
old = """  auto column_widths = headers |
                       std::views::transform([](const std::string& s) { return s.size(); }) |
                       std::ranges::to<std::vector>();
"""
new = """  std::vector<size_t> column_widths;
  column_widths.reserve(headers.size());
  std::ranges::transform(headers, std::back_inserter(column_widths),
                         [](const std::string& s) { return s.size(); });
"""
if old in text:
    write(cli_table, text.replace(old, new))

cli_util = Path("deps/anitomy/include/anitomy/detail/cli/util.hpp")
text = read(cli_util)
old = """    const auto values =
        elements |
        std::views::filter([&element](const auto& e) { return e.kind == element.kind; }) |
        std::views::transform([](const auto& e) { return e.value; }) |
        std::ranges::to<json::Value::array_t>();
"""
new = """    json::Value::array_t values;
    for (const auto& e : elements) {
      if (e.kind == element.kind) values.emplace_back(e.value);
    }
"""
if old in text:
    write(cli_util, text.replace(old, new))
PY
