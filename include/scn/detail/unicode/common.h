// Copyright 2017 Elias Kosunen
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

#ifndef SCN_DETAIL_UNICODE_COMMON_H
#define SCN_DETAIL_UNICODE_COMMON_H

#include "../string_view.h"

namespace scn {
    SCN_BEGIN_NAMESPACE

    enum class code_point : uint32_t {};

    template <typename T>
    constexpr bool operator==(code_point a, T b)
    {
        return static_cast<uint32_t>(a) == static_cast<uint32_t>(b);
    }
    template <typename T>
    constexpr bool operator!=(code_point a, T b)
    {
        return static_cast<uint32_t>(a) != static_cast<uint32_t>(b);
    }
    template <typename T>
    constexpr bool operator<(code_point a, T b)
    {
        return static_cast<uint32_t>(a) < static_cast<uint32_t>(b);
    }
    template <typename T>
    constexpr bool operator>(code_point a, T b)
    {
        return static_cast<uint32_t>(a) > static_cast<uint32_t>(b);
    }
    template <typename T>
    constexpr bool operator<=(code_point a, T b)
    {
        return static_cast<uint32_t>(a) <= static_cast<uint32_t>(b);
    }
    template <typename T>
    constexpr bool operator>=(code_point a, T b)
    {
        return static_cast<uint32_t>(a) >= static_cast<uint32_t>(b);
    }

    namespace detail {
        static constexpr const uint16_t lead_surrogate_min = 0xd800;
        static constexpr const uint16_t lead_surrogate_max = 0xdbff;
        static constexpr const uint16_t trail_surrogate_min = 0xdc00;
        static constexpr const uint16_t trail_surrogate_max = 0xdfff;
        static constexpr const uint16_t lead_offset =
            lead_surrogate_min - (0x10000u >> 10);
        static constexpr const uint32_t surrogate_offset =
            0x10000u - (lead_surrogate_min << 10) - trail_surrogate_min;
        static constexpr const uint32_t code_point_max = 0x10ffff;

        template <typename Octet>
        constexpr uint8_t mask8(Octet o)
        {
            return static_cast<uint8_t>(0xff & o);
        }
        template <typename U16>
        constexpr uint16_t mask16(U16 v)
        {
            return static_cast<uint16_t>(0xffff & v);
        }

        template <typename Octet>
        constexpr bool is_trail(Octet o)
        {
            return (mask8(o) >> 6) == 2;
        }
        template <typename U16>
        constexpr bool is_lead_surrogate(U16 cp)
        {
            return cp >= lead_surrogate_min && cp <= lead_surrogate_max;
        }
        template <typename U16>
        constexpr bool is_trail_surrogate(U16 cp)
        {
            return cp >= trail_surrogate_min && cp <= trail_surrogate_max;
        }
        template <typename U16>
        constexpr bool is_surrogate(U16 cp)
        {
            return cp >= lead_surrogate_min && cp <= trail_surrogate_max;
        }

        constexpr inline bool is_code_point_valid(code_point cp)
        {
            return cp <= code_point_max && !is_surrogate(cp);
        }
    }  // namespace detail

    template <typename T>
    constexpr code_point make_code_point(T ch)
    {
        return static_cast<code_point>(ch);
    }

    constexpr inline bool is_valid_code_point(code_point cp)
    {
        return detail::is_code_point_valid(cp);
    }
    constexpr inline bool is_ascii_code_point(code_point cp)
    {
        return cp <= 0x7f;
    }

    SCN_END_NAMESPACE
}  // namespace scn

#endif
