// Copyright 2017-2019 Elias Kosunen
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// This file is a part of scnlib:
//     https://github.com/eliaskosunen/scnlib

#ifndef SCN_DETAIL_SCAN_H
#define SCN_DETAIL_SCAN_H

#include <vector>

#include "vscan.h"

namespace scn {
    SCN_BEGIN_NAMESPACE

    namespace detail {
        template <typename Error, typename Range>
        using generic_scan_result_for_range = decltype(
            detail::wrap_result(std::declval<Error>(),
                                std::declval<detail::range_tag<Range>>(),
                                std::declval<range_wrapper_for_t<Range>>()));
        template <typename Range>
        using scan_result_for_range =
            generic_scan_result_for_range<wrapped_error, Range>;

        template <template <typename> class ParseCtx,
                  typename Range,
                  typename Format,
                  typename... Args>
        auto scan_boilerplate(Range&& r, const Format& f, Args&... a)
            -> detail::scan_result_for_range<Range>
        {
            static_assert(sizeof...(Args) > 0,
                          "Have to scan at least a single argument");
            static_assert(detail::ranges::range<Range>::value,
                          "Input needs to be a Range");

            using range_type = detail::range_wrapper_for_t<const Range&>;

            using context_type = basic_context<range_type>;
            using parse_context_type =
                ParseCtx<typename context_type::locale_type>;

            auto args = make_args<context_type, parse_context_type>(a...);
            auto ctx = context_type(detail::wrap(r));
            auto pctx = parse_context_type(f, ctx);
            auto err = vscan(ctx, pctx, {args});
            return detail::wrap_result(wrapped_error{err},
                                       detail::range_tag<Range>{},
                                       std::move(ctx.range()));
        }

        template <template <typename> class ParseCtx,
                  typename Locale,
                  typename Range,
                  typename Format,
                  typename... Args>
        auto scan_boilerplate_localized(const Locale& loc,
                                        Range&& r,
                                        const Format& f,
                                        Args&... a)
            -> detail::scan_result_for_range<Range>
        {
            static_assert(sizeof...(Args) > 0,
                          "Have to scan at least a single argument");
            static_assert(detail::ranges::range<Range>::value,
                          "Input needs to be a Range");

            using range_type = detail::range_wrapper_for_t<const Range&>;

            using context_type = basic_context<range_type, Locale>;
            using parse_context_type = ParseCtx<Locale>;

            auto args = make_args<context_type, parse_context_type>(a...);
            auto ctx = context_type(detail::wrap(r), {std::addressof(loc)});
            auto pctx = parse_context_type(f, ctx);
            auto err = vscan(ctx, pctx, {args});
            return detail::wrap_result(wrapped_error{err},
                                       detail::range_tag<Range>{},
                                       std::move(ctx.range()));
        }
    }  // namespace detail

    // scan

    /**
     * The most fundamental part of the scanning API.
     * Reads from the range in \c r according to the format string \c f.
     */
    template <typename Range, typename Format, typename... Args>
    auto scan(Range&& r, const Format& f, Args&... a)
        -> detail::scan_result_for_range<Range>
    {
        return detail::scan_boilerplate<basic_parse_context>(
            std::forward<Range>(r), f, a...);
    }

    // default format

    /**
     * Equivalent to \ref scan, but with a
     * format string with the appropriate amount of space-separated "{}"s for
     * the number of arguments. Because this function doesn't have to parse the
     * format string, performance is improved.
     *
     * \see scan
     */
    template <typename Range, typename... Args>
    auto scan_default(Range&& r, Args&... a)
        -> detail::scan_result_for_range<Range>
    {
        return detail::scan_boilerplate<basic_empty_parse_context>(
            std::forward<Range>(r), static_cast<int>(sizeof...(Args)), a...);
    }

    // scan localized

    /**
     * Read from the range in \c r using the locale in \c loc.
     * \c loc must be a std::locale.
     *
     * Use of this function is discouraged, due to the overhead involved with
     * locales. Note, that the other functions are completely locale-agnostic,
     * and aren't affected by changes to the global C locale.
     */
    template <typename Locale,
              typename Range,
              typename Format,
              typename... Args>
    auto scan_localized(const Locale& loc,
                        Range&& r,
                        const Format& f,
                        Args&... a) -> detail::scan_result_for_range<Range>
    {
        return detail::scan_boilerplate_localized<basic_parse_context>(
            loc, std::forward<Range>(r), f, a...);
    }

    // scanf

    /**
     * Otherwise equivalent to \ref scan, except it uses scanf-like format
     * string syntax, instead of the Python-like default one.
     */
    template <typename Range, typename Format, typename... Args>
    auto scanf(Range&& r, const Format& f, Args&... a)
        -> detail::scan_result_for_range<Range>
    {
        return detail::scan_boilerplate<basic_scanf_parse_context>(
            std::forward<Range>(r), f, a...);
    }

    // value

    /**
     * Scans a single value with the default options, returning it instead of
     * using an output parameter.
     *
     * The parsed value is in `ret.value()`, if `ret == true`.
     *
     * \code{.cpp}
     * auto ret = scn::scan_value<int>("42");
     * if (ret) {
     *   // ret.value() == 42
     * }
     * \endcode
     */
    template <typename T, typename Range>
    auto scan_value(Range&& r)
        -> detail::generic_scan_result_for_range<expected<T>, Range>
    {
        using range_type = detail::range_wrapper_for_t<Range>;
        using context_type = basic_context<range_type>;
        using parse_context_type =
            basic_empty_parse_context<typename context_type::locale_type>;

        T value;
        auto args = make_args<context_type, parse_context_type>(value);
        auto ctx = context_type(detail::wrap(r));

        auto pctx = parse_context_type(1, ctx);
        auto err = vscan(ctx, pctx, {args});
        if (!err) {
            return detail::wrap_result(expected<T>{err},
                                       detail::range_tag<Range>{},
                                       std::move(ctx.range()));
        }
        return detail::wrap_result(expected<T>{value},
                                   detail::range_tag<Range>{},
                                   std::move(ctx.range()));
    }

    // input

    /**
     * Otherwise equivalent to \ref scan, expect reads from `stdin`.
     * Character type is determined by the format string.
     *
     * Does not sync with the rest `<cstdio>` (like
     * `std::ios_base::sync_with_stdio(false)`). To use `<cstdio>` (like `fread`
     * or `fgets`) with this function, call `cstdin().sync()` or
     * `wcstdin().sync()` before calling `<cstdio>`.
     */
    template <typename Format,
              typename... Args,
              typename CharT = detail::ranges::range_value_t<Format>>
    auto input(const Format& f, Args&... a)
        -> detail::scan_result_for_range<decltype(stdin_range<CharT>().lock())>
    {
        return detail::scan_boilerplate<basic_parse_context>(
            std::move(stdin_range<CharT>().lock()), f, a...);
    }

    // prompt

    namespace detail {
        inline void put_stdout(const char* str)
        {
            std::fputs(str, stdout);
        }
        inline void put_stdout(const wchar_t* str)
        {
            std::fputws(str, stdout);
        }
    }  // namespace detail

    /**
     * Equivalent to \ref input, except writes what's in `p` to `stdout`.
     *
     * \code{.cpp}
     * int i{};
     * scn::prompt("What's your favorite number? ", "{}", i);
     * // Equivalent to:
     * //   std::fputs("What's your favorite number? ", stdout);
     * //   scn::input("{}", i);
     * \endcode
     */
    template <typename CharT, typename Format, typename... Args>
    auto prompt(const CharT* p, const Format& f, Args&... a)
        -> decltype(input(f, a...))
    {
        SCN_EXPECT(p != nullptr);
        detail::put_stdout(p);

        return input(f, a...);
    }

    // parse_integer

    /**
     * Parses an integer into `val` in base `base` from `str`.
     * Returns a pointer past the last character read, or an error.
     * `str` can't be empty, and cannot have:
     *   - preceding whitespace
     *   - preceding `"0x"` or `"0"` (base is determined by the `base`
     *     parameter)
     *   - `+` sign (`-` is fine)
     */
    template <typename T, typename CharT>
    expected<const CharT*> parse_integer(basic_string_view<CharT> str,
                                         T& val,
                                         int base = 10)
    {
        SCN_EXPECT(!str.empty());
        auto s = scanner<CharT, T>{base};
        bool minus_sign = false;
        if (str[0] == detail::ascii_widen<CharT>('-')) {
            minus_sign = true;
        }
        auto ret = s._read_int(val, minus_sign,
                               make_span(str.data(), str.size()).as_const(),
                               detail::ascii_widen<CharT>('\0'));
        if (!ret) {
            return ret.error();
        }
        return {ret.value()};
    }

    template <typename T, typename CharT>
    expected<const CharT*> parse_float(basic_string_view<CharT> str, T& val)
    {
        SCN_EXPECT(!str.empty());
        auto s = scanner<CharT, T>{};
        auto ret =
            s._read_int(val, make_span(str.data(), str.size()).as_const());
        if (!ret) {
            return ret.error();
        }
        return {ret.value()};
    }

    // scanning api

    // getline

    namespace detail {
        template <typename WrappedRange, typename String, typename CharT>
        error getline_impl(WrappedRange& r, String& str, CharT until)
        {
            auto until_pred = [until](CharT ch) { return ch == until; };
            auto s = read_until_space_zero_copy(r, until_pred, true);
            if (!s) {
                return s.error();
            }
            if (s.value().size() != 0) {
                auto size = s.value().size();
                if (until_pred(s.value()[size - 1])) {
                    --size;
                }
                str.clear();
                str.resize(size);
                std::copy(s.value().begin(), s.value().begin() + size,
                          str.begin());
                return {};
            }

            String tmp;
            auto out = std::back_inserter(tmp);
            auto e = read_until_space(r, out, until_pred, true);
            if (!e) {
                return e;
            }
            if (until_pred(tmp.back())) {
                tmp.pop_back();
            }
            str = std::move(tmp);
            return {};
        }
        template <typename WrappedRange, typename CharT>
        error getline_impl(WrappedRange& r,
                           basic_string_view<CharT>& str,
                           CharT until)
        {
            auto until_pred = [until](CharT ch) { return ch == until; };
            auto s = read_until_space_zero_copy(r, until_pred, true);
            if (!s) {
                return s.error();
            }
            if (s.value().size() != 0) {
                auto size = s.value().size();
                if (until_pred(s.value()[size - 1])) {
                    --size;
                }
                str = basic_string_view<CharT>{s.value().data(), size};
                return {};
            }
            // TODO: Compile-time error?
            return {error::invalid_operation,
                    "Cannot getline a string_view from a non-contiguous range"};
        }
#if SCN_HAS_STRING_VIEW
        template <typename WrappedRange, typename CharT>
        auto getline_impl(WrappedRange& r,
                          std::basic_string_view<CharT>& str,
                          CharT until) -> error
        {
            auto sv = ::scn::basic_string_view<CharT>{};
            auto ret = getline_impl(r, sv, until);
            str = ::std::basic_string_view<CharT>{sv.data(), sv.size()};
            return ret;
        }
#endif
    }  // namespace detail

    /**
     * Read the range in \c r into \c str until \c until is found.
     *
     * \c r and \c str must share character types, which must be \c CharT.
     *
     * If `str` is convertible to a `basic_string_view`:
     *  - And if `r` is a `contiguous_range`:
     *    - `str` is set to point inside `r` with the appropriate length
     *  - if not, returns an error
     *
     * Otherwise, clears `str` by calling `str.clear()`, and then reads the
     * range into `str` as if by repeatedly calling \c str.push_back.
     * `str.reserve` is also required to be present.
     */
    template <typename Range, typename String, typename CharT>
    auto getline(Range&& r, String& str, CharT until)
        -> detail::scan_result_for_range<Range>
    {
        auto wrapped = detail::wrap(r);
        auto err = getline_impl(wrapped, str, until);
        return detail::wrap_result(
            wrapped_error{err}, detail::range_tag<Range>{}, std::move(wrapped));
    }

    /**
     * Equivalent to \ref getline with the last parameter set to <tt>'\\n'</tt>
     * with the appropriate character type.
     *
     * In other words, reads `r` into `str` until <tt>'\\n'</tt> is found.
     *
     * The character type is determined by `r`.
     */
    template <typename Range,
              typename String,
              typename CharT =
                  typename detail::extract_char_type<detail::ranges::iterator_t<
                      detail::range_wrapper_for_t<Range>>>::type>
    auto getline(Range&& r, String& str) -> detail::scan_result_for_range<Range>
    {
        return getline(std::forward<Range>(r), str,
                       detail::ascii_widen<CharT>('\n'));
    }

    // ignore

    namespace detail {
        template <typename CharT>
        struct ignore_iterator {
            using value_type = CharT;
            using pointer = value_type*;
            using reference = value_type&;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::output_iterator_tag;

            constexpr ignore_iterator() = default;

            constexpr const ignore_iterator& operator=(CharT) const noexcept
            {
                return *this;
            }

            constexpr const ignore_iterator& operator*() const noexcept
            {
                return *this;
            }
            constexpr const ignore_iterator& operator++() const noexcept
            {
                return *this;
            }
        };

        template <typename CharT>
        struct ignore_iterator_n {
            using value_type = CharT;
            using pointer = value_type*;
            using reference = value_type&;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::output_iterator_tag;

            ignore_iterator_n() = default;
            ignore_iterator_n(difference_type n) : i(n) {}

            constexpr const ignore_iterator_n& operator=(CharT) const noexcept
            {
                return *this;
            }

            constexpr const ignore_iterator_n& operator*() const noexcept
            {
                return *this;
            }

            SCN_CONSTEXPR14 ignore_iterator_n& operator++() noexcept
            {
                ++i;
                return *this;
            }

            constexpr bool operator==(const ignore_iterator_n& o) const noexcept
            {
                return i == o.i;
            }
            constexpr bool operator!=(const ignore_iterator_n& o) const noexcept
            {
                return !(*this == o);
            }

            difference_type i{0};
        };

        template <typename WrappedRange,
                  typename CharT = typename detail::extract_char_type<
                      detail::range_wrapper_for_t<
                          typename WrappedRange::iterator>>::type>
        error ignore_until_impl(WrappedRange& r, CharT until)
        {
            auto until_pred = [until](CharT ch) { return ch == until; };
            ignore_iterator<CharT> it{};
            return read_until_space(r, it, until_pred, false);
        }

        template <typename WrappedRange,
                  typename CharT = typename detail::extract_char_type<
                      detail::range_wrapper_for_t<
                          typename WrappedRange::iterator>>::type>
        error ignore_until_n_impl(WrappedRange& r,
                                  ranges::range_difference_t<WrappedRange> n,
                                  CharT until)
        {
            auto until_pred = [until](CharT ch) { return ch == until; };
            ignore_iterator_n<CharT> begin{}, end{n};
            return read_until_space_ranged(r, begin, end, until_pred, false);
        }
    }  // namespace detail

    /**
     * Advances the beginning of \c r until \c until is found.
     * The character type of \c r must be \c CharT.
     */
    template <typename Range, typename CharT>
    auto ignore_until(Range&& r, CharT until)
        -> detail::scan_result_for_range<Range>
    {
        auto wrapped = detail::wrap(r);
        auto err = detail::ignore_until_impl(wrapped, until);
        if (!err) {
            auto e = wrapped.reset_to_rollback_point();
            if (!e) {
                err = e;
            }
        }
        return detail::wrap_result(
            wrapped_error{err}, detail::range_tag<Range>{}, std::move(wrapped));
    }

    /**
     * Advances the beginning of \c r until \c until is found, or the beginning
     * has been advanced \c n times. The character type of \c r must be \c
     * CharT.
     */
    template <typename Range, typename CharT>
    auto ignore_until_n(Range&& r,
                        detail::ranges::range_difference_t<Range> n,
                        CharT until) -> detail::scan_result_for_range<Range>
    {
        auto wrapped = detail::wrap(r);
        auto err = detail::ignore_until_n_impl(wrapped, n, until);
        if (!err) {
            auto e = wrapped.reset_to_rollback_point();
            if (!e) {
                err = e;
            }
        }
        return detail::wrap_result(
            wrapped_error{err}, detail::range_tag<Range>{}, std::move(wrapped));
    }

    template <typename T>
    struct span_list_wrapper {
        using value_type = T;

        span_list_wrapper(span<T> s) : m_span(s) {}

        void push_back(T val)
        {
            SCN_EXPECT(n < max_size());
            m_span[n] = std::move(val);
            ++n;
        }

        std::size_t size() const
        {
            return n;
        }
        std::size_t max_size() const
        {
            return m_span.size();
        }

        span<T> m_span;
        std::size_t n{0};
    };
    template <typename T>
    auto make_span_list_wrapper(T& s) -> temporary<
        span_list_wrapper<typename decltype(make_span(s))::value_type>>
    {
        auto _s = make_span(s);
        return temp(span_list_wrapper<typename decltype(_s)::value_type>(_s));
    }

    namespace detail {
        template <typename CharT>
        struct zero_value;
        template <>
        struct zero_value<char> : std::integral_constant<char, 0> {
        };
        template <>
        struct zero_value<wchar_t> : std::integral_constant<wchar_t, 0> {
        };
    }  // namespace detail

    /**
     * Reads values repeatedly from `r` and writes them into `c`.
     * The values read are of type `Container::value_type`, and they are written
     * into `c` using `c.push_back`. The values must be separated by separator
     * character `separator`, followed by whitespace. If `separator == 0`, no
     * separator character is expected.
     *
     * To scan a `span`, use `span_list_wrapper`.
     */
    template <typename Range,
              typename Container,
              typename CharT = typename detail::extract_char_type<
                  detail::ranges::iterator_t<Range>>::type>
    auto scan_list(Range&& r,
                   Container& c,
                   CharT separator = detail::zero_value<CharT>::value)
        -> detail::scan_result_for_range<Range>
    {
        using value_type = typename Container::value_type;
        using range_type = detail::range_wrapper_for_t<Range>;
        using context_type = basic_context<range_type>;
        using parse_context_type =
            basic_empty_parse_context<typename context_type::locale_type>;

        value_type value;
        auto args = make_args<context_type, parse_context_type>(value);
        auto ctx = context_type(detail::wrap(r));

        while (true) {
            if (c.size() == c.max_size()) {
                break;
            }

            auto pctx = parse_context_type(1, ctx);
            auto err = vscan(ctx, pctx, {args});
            if (!err) {
                if (err == error::end_of_range) {
                    break;
                }
                return detail::wrap_result(wrapped_error{err},
                                           detail::range_tag<Range>{},
                                           std::move(ctx.range()));
            }
            c.push_back(std::move(value));

            if (separator != 0) {
                auto sep_ret = read_char(ctx.range());
                if (!sep_ret) {
                    if (sep_ret.error() == scn::error::end_of_range) {
                        break;
                    }
                    return detail::wrap_result(wrapped_error{sep_ret.error()},
                                               detail::range_tag<Range>{},
                                               std::move(ctx.range()));
                }
                if (sep_ret.value() == separator) {
                    continue;
                }
                else {
                    // Unexpected character, assuming end
                    break;
                }
            }
        }
        return detail::wrap_result(wrapped_error{}, detail::range_tag<Range>{},
                                   std::move(ctx.range()));
    }

    template <typename Range,
              typename Container,
              typename CharT = typename detail::extract_char_type<
                  detail::ranges::iterator_t<Range>>::type>
    auto scan_list_until(Range&& r,
                         Container& c,
                         CharT until,
                         CharT separator = detail::zero_value<CharT>::value)
        -> detail::scan_result_for_range<Range>
    {
        using value_type = typename Container::value_type;
        using range_type = detail::range_wrapper_for_t<Range>;
        using context_type = basic_context<range_type>;
        using parse_context_type =
            basic_empty_parse_context<typename context_type::locale_type>;

        value_type value;
        auto args = make_args<context_type, parse_context_type>(value);
        auto ctx = context_type(detail::wrap(std::forward<Range>(r)));

        while (true) {
            if (c.size() == c.max_size()) {
                break;
            }

            auto pctx = parse_context_type(1, ctx);
            auto err = vscan(ctx, pctx, {args});
            if (!err) {
                if (err == error::end_of_range) {
                    break;
                }
                return detail::wrap_result(wrapped_error{err},
                                           detail::range_tag<Range>{},
                                           std::move(ctx.range()));
            }
            c.push_back(std::move(value));

            {
                auto next = read_char(ctx.range());
                if (!next) {
                    if (next.error() == scn::error::end_of_range) {
                        break;
                    }
                    return detail::wrap_result(wrapped_error{next.error()},
                                               detail::range_tag<Range>{},
                                               std::move(ctx.range()));
                }
                if (next.value() == until) {
                    break;
                }
                if (separator != 0) {
                    if (next.value() != separator) {
                        break;
                    }
                }
                else {
                    if (!ctx.locale().is_space(next.value())) {
                        break;
                    }
                }
                next = read_char(ctx.range());
                if (next.value() == until) {
                    break;
                }
                else {
                    putback_n(ctx.range(), 1);
                }
            }
        }
        return detail::wrap_result(wrapped_error{}, detail::range_tag<Range>{},
                                   std::move(ctx.range()));
    }

    template <typename T>
    struct discard_type {
        discard_type() = default;
    };

    /**
     * Scans an instance of `T`, but doesn't store it anywhere.
     * Uses `scn::temp` internally, so the user doesn't have to bother.
     *
     * \code{.cpp}
     * int i{};
     * // 123 is discarded, 456 is read into `i`
     * auto ret = scn::scan("123 456", "{} {}",
     *     discard<T>(), i);
     * // ret == true
     * // ret.value() == 2
     * // i == 456
     * \endcode
     */
    template <typename T>
    discard_type<T>& discard()
    {
        return temp(discard_type<T>{})();
    }

    template <typename CharT, typename T>
    struct scanner<CharT, discard_type<T>> : public scanner<CharT, T> {
        template <typename Context>
        error scan(discard_type<T>&, Context& ctx)
        {
            T tmp;
            return scanner<CharT, T>::scan(tmp, ctx);
        }
    };

    SCN_END_NAMESPACE
}  // namespace scn

#endif  // SCN_DETAIL_SCAN_H
