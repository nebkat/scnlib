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

#ifndef SCN_DETAIL_RANGE_H
#define SCN_DETAIL_RANGE_H

#include "ranges.h"
#include "result.h"

namespace scn {
    SCN_BEGIN_NAMESPACE

    namespace detail {
        template <typename Iterator, typename = void>
        struct extract_char_type;
        template <typename Iterator>
        struct extract_char_type<
            Iterator,
            typename std::enable_if<std::is_integral<
                ranges::iter_value_t<Iterator>>::value>::type> {
            using type = ranges::iter_value_t<Iterator>;
        };
        template <typename Iterator>
        struct extract_char_type<
            Iterator,
            void_t<typename std::enable_if<!std::is_integral<
                       ranges::iter_value_t<Iterator>>::value>::type,
                   typename ranges::iter_value_t<Iterator>::success_type>> {
            using type = typename ranges::iter_value_t<Iterator>::success_type;
        };

        template <typename Range, typename = void>
        struct is_direct_impl
            : std::is_integral<ranges::range_value_t<const Range>> {
        };
        template <typename Range, typename = void>
        struct is_contiguous_impl : ranges::contiguous_range<Range> {
        };
        template <typename Range, typename = void>
        struct provides_buffer_access_impl : std::false_type {
        };

        template <typename Range>
        struct reconstruct_tag {
        };

        template <typename Range, typename Iterator, typename Sentinel>
        Range reconstruct(reconstruct_tag<Range>, Iterator begin, Sentinel end)
        {
            return {begin, end};
        }
#if SCN_HAS_STRING_VIEW
        // std::string_view is not reconstructible pre-C++20
        template <typename CharT,
                  typename Traits,
                  typename Iterator,
                  typename Sentinel>
        std::basic_string_view<CharT, Traits> reconstruct(
            reconstruct_tag<std::basic_string_view<CharT, Traits>>,
            Iterator begin,
            Sentinel end)
        {
            // On MSVC, string_view can't even be constructed from its
            // iterators!
            return {::scn::detail::to_address(begin),
                    static_cast<size_t>(ranges::distance(begin, end))};
        }
#endif  // SCN_HAS_STRING_VIEW

        template <typename Range>
        struct is_caching_range_impl : std::false_type {
        };
        template <typename Range>
        struct is_caching_range : is_caching_range_impl<remove_cvref_t<Range>> {
        };

        template <typename T, bool>
        struct range_wrapper_storage;
        template <typename T>
        struct range_wrapper_storage<T, true> {
            using type = remove_cvref_t<T>;
            using range_type = const type&;

            const type* value{nullptr};

            range_wrapper_storage() = default;
            range_wrapper_storage(const type& v) : value(std::addressof(v)) {}

            const type& get() const&
            {
                return *value;
            }
            type&& get() &&
            {
                return std::move(*value);
            }
        };
        template <typename T>
        struct range_wrapper_storage<T, false> {
            using range_type = T;

            T value{};

            range_wrapper_storage() = default;
            template <typename U>
            range_wrapper_storage(U&& v) : value(std::forward<U>(v))
            {
            }

            const T& get() const&
            {
                return value;
            }
            T&& get() &&
            {
                return std::move(value);
            }
        };

        template <typename T>
        using _range_wrapper_marker = typename T::range_wrapper_marker;

        template <typename T>
        struct _has_range_wrapper_marker : exists<_range_wrapper_marker, T> {
        };

        template <typename Range>
        class range_wrapper {
        public:
            using range_type = Range;
            using iterator = ranges::iterator_t<const Range>;
            using sentinel = ranges::sentinel_t<const Range>;
            using char_type = typename extract_char_type<iterator>::type;
            using difference_type = ranges::range_difference_t<Range>;
            using storage_type =
                range_wrapper_storage<Range, std::is_reference<Range>::value>;
            using storage_range_type = typename storage_type::range_type;

            using range_wrapper_marker = void;

            template <
                typename R,
                typename = typename std::enable_if<
                    !_has_range_wrapper_marker<remove_cvref_t<R>>::value>::type>
            range_wrapper(R&& r)
                : m_range(std::forward<R>(r)),
                  m_begin(ranges::begin(m_range.get()))
            {
            }

            range_wrapper(const range_wrapper& o) : m_range(o.m_range)
            {
                const auto n =
                    ranges::distance(o.begin_underlying(), o.m_begin);
                m_begin = ranges::begin(m_range.get());
                ranges::advance(m_begin, n);
                m_read = o.m_read;
            }
            range_wrapper& operator=(const range_wrapper& o)
            {
                const auto n =
                    ranges::distance(o.begin_underlying(), o.m_begin);
                m_range = o.m_range;
                m_begin = ranges::begin(m_range.get());
                ranges::advance(m_begin, n);
                m_read = o.m_read;
                return *this;
            }

            range_wrapper(range_wrapper&& o) noexcept
            {
                const auto n =
                    ranges::distance(o.begin_underlying(), o.m_begin);
                m_range = std::move(o.m_range);
                m_begin = ranges::begin(m_range.get());
                ranges::advance(m_begin, n);
                m_read = exchange(o.m_read, 0);
            }
            range_wrapper& operator=(range_wrapper&& o)
            {
                reset_to_rollback_point();

                const auto n =
                    ranges::distance(o.begin_underlying(), o.m_begin);
                m_range = std::move(o.m_range);
                m_begin = ranges::begin(m_range.get());
                ranges::advance(m_begin, n);
                m_read = exchange(o.m_read, 0);
                return *this;
            }

            ~range_wrapper() = default;

            iterator begin() const noexcept
            {
                return m_begin;
            }
            sentinel end() const
                noexcept(noexcept(ranges::end(std::declval<const Range&>())))
            {
                return ranges::end(m_range.get());
            }

            iterator advance(difference_type n = 1) noexcept
            {
                m_read += n;
                if (!is_caching_range<Range>::value) {
                    ranges::advance(m_begin, n);
                }
                return m_begin;
            }
            template <typename R = Range,
                      typename std::enable_if<
                          ranges::sized_range<R>::value>::type* = nullptr>
            void advance_to(iterator it) noexcept
            {
                const auto diff = ranges::distance(m_begin, it);
                m_read += diff;
                m_begin = it;
            }

            iterator begin_underlying() const
                noexcept(noexcept(ranges::begin(std::declval<const Range&>())))
            {
                return ranges::begin(m_range.get());
            }

            const range_type& range_underlying() const noexcept
            {
                return m_range.get();
            }

            template <typename R = Range,
                      typename std::enable_if<
                          ranges::contiguous_range<R>::value>::type* = nullptr>
            auto data() const
                noexcept(noexcept(*std::declval<ranges::iterator_t<const R>>()))
                    -> decltype(std::addressof(
                        *std::declval<ranges::iterator_t<const R>>()))
            {
                return std::addressof(*m_begin);
            }
            template <typename R = Range,
                      typename std::enable_if<
                          ranges::sized_range<R>::value>::type* = nullptr>
            auto size() const noexcept(noexcept(
                ranges::distance(std::declval<ranges::iterator_t<const R>>(),
                                 std::declval<ranges::sentinel_t<const R>>())))
                -> decltype(ranges::distance(
                    std::declval<ranges::iterator_t<const R>>(),
                    std::declval<ranges::sentinel_t<const R>>()))
            {
                return ranges::distance(m_begin, end());
            }

            error reset_to_rollback_point()
            {
                for (; m_read != 0; --m_read) {
                    --m_begin;
                    if (m_begin == end()) {
                        return error(error::unrecoverable_source_error,
                                     "Putback failed");
                    }
                }
                return {};
            }
            void set_rollback_point()
            {
                m_read = 0;
            }

            template <typename R = Range,
                      typename std::enable_if<std::is_same<
                          remove_cvref_t<Range>,
                          remove_cvref_t<R>>::value>::type* = nullptr>
            auto rewrap() const& -> range_wrapper<R>
            {
                const auto n = ranges::distance(begin_underlying(), begin());
                auto r = range_wrapper<R>{m_range.get()};
                r.advance(n);
                r.set_rollback_point();
                return r;
            }
            template <typename R = Range,
                      typename std::enable_if<!std::is_same<
                          remove_cvref_t<Range>,
                          remove_cvref_t<R>>::value>::type* = nullptr>
            auto rewrap() const& -> range_wrapper<R>
            {
                return {reconstruct(reconstruct_tag<R>{}, begin(), end())};
            }

            template <typename R = Range,
                      typename std::enable_if<std::is_same<
                          remove_cvref_t<Range>,
                          remove_cvref_t<R>>::value>::type* = nullptr>
            auto rewrap() && -> range_wrapper<R>
            {
                const auto n = ranges::distance(begin_underlying(), begin());
                auto r = range_wrapper<R>{std::move(m_range.get())};
                r.advance(n);
                r.set_rollback_point();
                return r;
            }
            template <typename R = Range,
                      typename std::enable_if<!std::is_same<
                          remove_cvref_t<Range>,
                          remove_cvref_t<R>>::value>::type* = nullptr>
            auto rewrap() && -> range_wrapper<R>
            {
                return {reconstruct(reconstruct_tag<R>{}, begin(), end())};
            }

            // iterator value type is a character
            static constexpr bool is_direct = is_direct_impl<Range>::value;
            // can call .data() and memcpy
            static constexpr bool is_contiguous =
                is_contiguous_impl<Range>::value;
            // provides mechanism to get a pointer to memcpy from
            static constexpr bool provides_buffer_access =
                provides_buffer_access_impl<Range>::value;

        private:
            storage_type m_range;
            iterator m_begin;
            difference_type m_read{0};
        };

        namespace _wrap {
            struct fn {
            private:
                template <typename Range>
                static range_wrapper<Range> impl(const range_wrapper<Range>& r,
                                                 priority_tag<3>) noexcept
                {
                    return r;
                }
                template <typename Range>
                static range_wrapper<Range> impl(range_wrapper<Range>&& r,
                                                 priority_tag<3>) noexcept
                {
                    return r;
                }

                template <typename Range>
                static auto impl(const Range& r,
                                 priority_tag<2>) noexcept(noexcept(r.wrap()))
                    -> decltype(r.wrap())
                {
                    return r.wrap();
                }

                template <typename CharT, std::size_t N>
                static auto impl(CharT (&str)[N], priority_tag<1>) noexcept
                    -> range_wrapper<
                        basic_string_view<typename std::remove_cv<CharT>::type>>
                {
                    return {
                        basic_string_view<typename std::remove_cv<CharT>::type>(
                            str, str + N - 1)};
                }

                template <typename CharT, typename Allocator>
                static auto impl(
                    const std::basic_string<CharT,
                                            std::char_traits<CharT>,
                                            Allocator>& str,
                    priority_tag<1>) noexcept
                    -> range_wrapper<basic_string_view<CharT>>
                {
                    return {basic_string_view<CharT>{str.data(), str.size()}};
                }
#if SCN_HAS_STRING_VIEW
                template <typename CharT>
                static auto impl(const std::basic_string_view<CharT>& str,
                                 priority_tag<1>) noexcept
                    -> range_wrapper<basic_string_view<CharT>>
                {
                    return {basic_string_view<CharT>{str.data(), str.size()}};
                }
#endif
                template <typename CharT>
                static auto impl(span<const CharT> s, priority_tag<1>) noexcept
                    -> range_wrapper<basic_string_view<CharT>>
                {
                    return {basic_string_view<CharT>{s.data(), s.size()}};
                }

                template <typename Range>
                static auto impl(const Range& r, priority_tag<0>) noexcept
                    -> range_wrapper<Range&>
                {
                    static_assert(ranges::range<Range>::value,
                                  "Input needs to be a Range");
                    return {r};
                }

            public:
                template <typename Range>
                auto operator()(Range&& r) const
                    noexcept(noexcept(fn::impl(std::forward<Range>(r),
                                               priority_tag<3>{})))
                        -> decltype(fn::impl(std::forward<Range>(r),
                                             priority_tag<3>{}))
                {
                    return fn::impl(std::forward<Range>(r), priority_tag<3>{});
                }
            };
        }  // namespace _wrap
        namespace {
            static constexpr auto& wrap = static_const<_wrap::fn>::value;
        }

        template <typename Range>
        struct range_wrapper_for {
            using type = decltype(wrap(std::declval<Range>()));
        };
        template <typename Range>
        using range_wrapper_for_t = typename range_wrapper_for<Range>::type;
    }  // namespace detail

    struct wrapped_error {
        wrapped_error() = default;
        wrapped_error(::scn::error e) : err(e) {}

        ::scn::error error() const
        {
            return err;
        }

        explicit operator bool() const
        {
            return err.operator bool();
        }

        ::scn::error err{};
    };

    namespace detail {
        template <typename Base>
        class scan_result_base_wrapper : public Base {
        public:
            scan_result_base_wrapper(Base&& b) : Base(std::move(b)) {}

        protected:
            void set_base(const Base& b)
            {
                static_cast<Base&>(*this) = b;
            }
            void set_base(Base&& b)
            {
                static_cast<Base&>(*this) = std::move(b);
            }
        };

        template <typename WrappedRange, typename Base>
        class scan_result_base : public scan_result_base_wrapper<Base> {
        public:
            using wrapped_range_type = WrappedRange;
            using base_type = scan_result_base_wrapper<Base>;

            using range_type = typename wrapped_range_type::range_type;
            using iterator = typename wrapped_range_type::iterator;
            using sentinel = typename wrapped_range_type::sentinel;
            using char_type = typename wrapped_range_type::char_type;

            scan_result_base(Base&& b, wrapped_range_type&& r)
                : base_type(std::move(b)), m_range(std::move(r))
            {
            }

            iterator begin() const noexcept
            {
                return m_range.begin();
            }
            sentinel end() const
                noexcept(noexcept(std::declval<wrapped_range_type>().end()))
            {
                return m_range.end();
            }

            detail::ranges::subrange<iterator, sentinel> subrange() const
            {
                return {begin(), end()};
            }

            wrapped_range_type& range() &
            {
                return m_range;
            }
            const wrapped_range_type& range() const&
            {
                return m_range;
            }
            wrapped_range_type range() &&
            {
                return std::move(m_range);
            }

            wrapped_range_type wrap() const
            {
                return m_range;
            }

            template <
                typename R = wrapped_range_type,
                typename = typename std::enable_if<R::is_contiguous>::type>
            basic_string_view<char_type> string_view() const
            {
                return {m_range.data(),
                        static_cast<std::size_t>(m_range.size())};
            }
            template <
                typename R = wrapped_range_type,
                typename = typename std::enable_if<R::is_contiguous>::type>
            span<char_type> span() const
            {
                return {m_range.data(),
                        static_cast<std::size_t>(m_range.size())};
            }
            template <
                typename R = wrapped_range_type,
                typename = typename std::enable_if<R::is_contiguous>::type>
            std::basic_string<char_type> string() const
            {
                return {m_range.data(),
                        static_cast<std::size_t>(m_range.size())};
            }

        protected:
            wrapped_range_type m_range;
        };

        template <typename WrappedRange, typename Base>
        class reconstructed_scan_result
            : public scan_result_base<WrappedRange, Base> {
        public:
            using unwrapped_range_type = typename WrappedRange::range_type;
            using base_type = scan_result_base<WrappedRange, Base>;

            reconstructed_scan_result(Base&& b, WrappedRange&& r)
                : base_type(std::move(b), std::move(r))
            {
            }

            unwrapped_range_type reconstruct() const
            {
                return this->range().range_underlying();
            }
        };
        template <typename WrappedRange, typename UnwrappedRange, typename Base>
        class non_reconstructed_scan_result
            : public scan_result_base<WrappedRange, Base> {
        public:
            using unwrapped_range_type = UnwrappedRange;
            using base_type = scan_result_base<WrappedRange, Base>;

            non_reconstructed_scan_result(Base&& b, WrappedRange&& r)
                : base_type(std::move(b), std::move(r))
            {
            }

            non_reconstructed_scan_result& operator=(
                const reconstructed_scan_result<WrappedRange, Base>& other)
            {
                this->set_base(other);
                this->m_range = other.range();
                return *this;
            }
            non_reconstructed_scan_result& operator=(
                reconstructed_scan_result<WrappedRange, Base>&& other)
            {
                this->set_base(other);
                this->m_range = std::move(other).range();
                return *this;
            }

            template <typename R = unwrapped_range_type>
            R reconstruct() const
            {
                return reconstruct(reconstruct_tag<R>{}, this->begin(),
                                   this->end());
            }
        };

        template <typename T>
        struct range_tag {
        };

        namespace _wrap_result {
            struct fn {
            private:
                // Range = range_wrapper&
                template <typename Error,
                          typename Range,
                          typename = typename std::enable_if<
                              !std::is_lvalue_reference<Range>::value>::type>
                static auto impl(Error e,
                                 range_tag<range_wrapper<Range>&>,
                                 range_wrapper<Range>&& range,
                                 priority_tag<3>)
                    -> reconstructed_scan_result<range_wrapper<Range>, Error>
                {
                    return {std::move(e), std::move(range)};
                }
                // Range = const range_wrapper&
                template <typename Error,
                          typename Range,
                          typename = typename std::enable_if<
                              !std::is_lvalue_reference<Range>::value>::type>
                static auto impl(Error e,
                                 range_tag<const range_wrapper<Range>&>,
                                 range_wrapper<Range>&& range,
                                 priority_tag<3>)
                    -> reconstructed_scan_result<range_wrapper<Range>, Error>
                {
                    return {std::move(e), std::move(range)};
                }
                // Range = range_wrapper&&
                template <typename Error,
                          typename Range,
                          typename = typename std::enable_if<
                              !std::is_lvalue_reference<Range>::value>::type>
                static auto impl(Error e,
                                 range_tag<range_wrapper<Range>>,
                                 range_wrapper<Range>&& range,
                                 priority_tag<3>)
                    -> reconstructed_scan_result<range_wrapper<Range>, Error>
                {
                    return {std::move(e), std::move(range)};
                }

                // string literals are wonky
                template <typename Error,
                          typename CharT,
                          size_t N,
                          typename NoCvref = remove_cvref_t<CharT>>
                static auto impl(
                    Error e,
                    range_tag<CharT (&)[N]>,
                    range_wrapper<basic_string_view<NoCvref>>&& range,
                    priority_tag<2>)
                    -> reconstructed_scan_result<
                        range_wrapper<basic_string_view<NoCvref>>,
                        Error>
                {
                    return {std::move(e), std::move(range)};
                }

                // (const) InputRange&
                // wrapped<non-ref>
                template <typename Error,
                          typename InputRange,
                          typename InnerWrappedRange,
                          typename =
                              typename std::enable_if<!std::is_lvalue_reference<
                                  InnerWrappedRange>::value>::type>
                static auto impl(Error e,
                                 range_tag<InputRange&>,
                                 range_wrapper<InnerWrappedRange>&& range,
                                 priority_tag<1>)
                    -> non_reconstructed_scan_result<
                        range_wrapper<InnerWrappedRange>,
                        typename std::remove_const<InputRange>::type,
                        Error>
                {
                    return {std::move(e), std::move(range)};
                }

                // (const) InputRange&
                // wrapped<ref>
                template <typename Error,
                          typename InputRange,
                          typename InnerWrappedRange,
                          typename NoRef = typename std::remove_reference<
                              InnerWrappedRange>::type>
                static auto impl(Error e,
                                 range_tag<InputRange&>,
                                 range_wrapper<InnerWrappedRange&>&& range,
                                 priority_tag<1>)
                    -> reconstructed_scan_result<range_wrapper<NoRef>, Error>
                {
                    return {std::move(e),
                            std::move(range).template rewrap<NoRef>()};
                }

                // InputRange&&
                // wrapped<non-ref>
                template <typename Error,
                          typename InputRange,
                          typename InnerWrappedRange,
                          typename =
                              typename std::enable_if<!std::is_lvalue_reference<
                                  InnerWrappedRange>::value>::type>
                static auto impl(Error e,
                                 range_tag<InputRange>,
                                 range_wrapper<InnerWrappedRange>&& range,
                                 priority_tag<0>)
                    -> reconstructed_scan_result<range_wrapper<InputRange>,
                                                 Error>
                {
                    return {std::move(e),
                            std::move(range).template rewrap<InputRange>()};
                }

                // InputRange&&
                // wrapped<ref>
                template <typename Error,
                          typename InputRange,
                          typename InnerWrappedRange,
                          typename NoRef = typename std::remove_reference<
                              InnerWrappedRange>::type>
                static auto impl(Error e,
                                 range_tag<InputRange>,
                                 range_wrapper<InnerWrappedRange&>&& range,
                                 priority_tag<0>)
                    -> reconstructed_scan_result<range_wrapper<NoRef>, Error>
                {
                    return {std::move(e),
                            std::move(range).template rewrap<NoRef>()};
                }

            public:
                template <typename Error,
                          typename InputRange,
                          typename InnerWrappedRange>
                auto operator()(Error e,
                                range_tag<InputRange> tag,
                                range_wrapper<InnerWrappedRange>&& range) const
                    noexcept(noexcept(impl(std::move(e),
                                           tag,
                                           std::move(range),
                                           priority_tag<3>{})))
                        -> decltype(impl(std::move(e),
                                         tag,
                                         std::move(range),
                                         priority_tag<3>{}))
                {
                    static_assert(ranges::range<InputRange>::value,
                                  "Input needs to be a Range");
                    return impl(std::move(e), tag, std::move(range),
                                priority_tag<3>{});
                }
            };
        }  // namespace _wrap_result
        namespace {
            static constexpr auto& wrap_result =
                static_const<_wrap_result::fn>::value;
        }

        template <typename Error, typename InputRange, typename WrappedRange>
        struct result_type_for {
            using type =
                decltype(wrap_result(std::declval<Error&&>(),
                                     std::declval<range_tag<InputRange>>(),
                                     std::declval<WrappedRange&&>()));
        };
        template <typename Error, typename InputRange, typename WrappedRange>
        using result_type_for_t =
            typename result_type_for<Error, InputRange, WrappedRange>::type;
    }  // namespace detail

    template <typename Error = wrapped_error, typename Range>
    auto make_result(Range&& r)
    {
        return detail::wrap_result(Error{}, detail::range_tag<Range>{},
                                   detail::wrap(r));
    }

    SCN_END_NAMESPACE
}  // namespace scn

#endif  // SCN_DETAIL_RANGE_H
