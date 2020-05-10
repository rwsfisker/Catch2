// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef TWOBLUECUBES_CATCH_MATCHERS_TEMPLATED_HPP_INCLUDED
#define TWOBLUECUBES_CATCH_MATCHERS_TEMPLATED_HPP_INCLUDED

#include <catch2/internal/catch_common.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/internal/catch_stringref.hpp>

#include <array>
#include <algorithm>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace Catch {
namespace Matchers {
    struct MatcherGenericBase : MatcherUntypedBase {
        virtual ~MatcherGenericBase(); // = default;
    };


    namespace Detail {
        template<std::size_t N, std::size_t M>
        std::array<void const*, N + M> array_cat(std::array<void const*, N> && lhs, std::array<void const*, M> && rhs) {
            std::array<void const*, N + M> arr{};
            std::copy_n(lhs.begin(), N, arr.begin());
            std::copy_n(rhs.begin(), M, arr.begin() + N);
            return arr;
        }

        template<std::size_t N>
        std::array<void const*, N+1> array_cat(std::array<void const*, N> && lhs, void const* rhs) {
            std::array<void const*, N+1> arr{};
            std::copy_n(lhs.begin(), N, arr.begin());
            arr[N] = rhs;
            return arr;
        }

        template<std::size_t N>
        std::array<void const*, N+1> array_cat(void const* lhs, std::array<void const*, N> && rhs) {
            std::array<void const*, N + 1> arr{ {lhs} };
            std::copy_n(rhs.begin(), N, arr.begin() + 1);
            return arr;
        }

        #ifdef CATCH_CPP17_OR_GREATER

        using std::conjunction;

        #else // CATCH_CPP17_OR_GREATER

        template<typename... Cond>
        struct conjunction : std::true_type {};

        template<typename Cond, typename... Rest>
        struct conjunction<Cond, Rest...> : std::integral_constant<bool, Cond::value && conjunction<Rest...>::value> {};

        #endif // CATCH_CPP17_OR_GREATER

        template<typename T>
        using is_generic_matcher = std::is_base_of<
            Catch::Matchers::MatcherGenericBase,
            std::remove_cv_t<std::remove_reference_t<T>>
        >;

        template<typename... Ts>
        using are_generic_matchers = conjunction<is_generic_matcher<Ts>...>;

        template<typename T>
        using is_matcher = std::is_base_of<
            Catch::Matchers::MatcherUntypedBase,
            std::remove_cv_t<std::remove_reference_t<T>>
        >;


        template<std::size_t N, typename Arg>
        bool match_all_of(Arg&&, std::array<void const*, N> const&, std::index_sequence<>) {
            return true;
        }

        template<typename T, typename... MatcherTs, std::size_t N, typename Arg, std::size_t Idx, std::size_t... Indices>
        bool match_all_of(Arg&& arg, std::array<void const*, N> const& matchers, std::index_sequence<Idx, Indices...>) {
            return static_cast<T const*>(matchers[Idx])->match(arg) && match_all_of<MatcherTs...>(arg, matchers, std::index_sequence<Indices...>{});
        }


        template<std::size_t N, typename Arg>
        bool match_any_of(Arg&&, std::array<void const*, N> const&, std::index_sequence<>) {
            return false;
        }

        template<typename T, typename... MatcherTs, std::size_t N, typename Arg, std::size_t Idx, std::size_t... Indices>
        bool match_any_of(Arg&& arg, std::array<void const*, N> const& matchers, std::index_sequence<Idx, Indices...>) {
            return static_cast<T const*>(matchers[Idx])->match(arg) || match_any_of<MatcherTs...>(arg, matchers, std::index_sequence<Indices...>{});
        }

        std::string describe_multi_matcher(StringRef combine, std::string const* descriptions_begin, std::string const* descriptions_end);

        template<typename... MatcherTs, std::size_t... Idx>
        std::string describe_multi_matcher(StringRef combine, std::array<void const*, sizeof...(MatcherTs)> const& matchers, std::index_sequence<Idx...>) {
            std::array<std::string, sizeof...(MatcherTs)> descriptions {{
                static_cast<MatcherTs const*>(matchers[Idx])->toString()...
            }};

            return describe_multi_matcher(combine, descriptions.data(), descriptions.data() + descriptions.size());
        }


        template<typename... MatcherTs>
        struct MatchAllOfGeneric final : MatcherGenericBase {
            MatchAllOfGeneric(MatchAllOfGeneric const&) = delete;
            MatchAllOfGeneric& operator=(MatchAllOfGeneric const&) = delete;
            MatchAllOfGeneric(MatchAllOfGeneric&&) = default;
            MatchAllOfGeneric& operator=(MatchAllOfGeneric&&) = default;

            MatchAllOfGeneric(MatcherTs const&... matchers) : m_matchers{ {std::addressof(matchers)...} } {}
            explicit MatchAllOfGeneric(std::array<void const*, sizeof...(MatcherTs)> matchers) : m_matchers{matchers} {}

            template<typename Arg>
            bool match(Arg&& arg) const {
                return match_all_of<MatcherTs...>(arg, m_matchers, std::index_sequence_for<MatcherTs...>{});
            }

            std::string describe() const override {
                return describe_multi_matcher<MatcherTs...>(" and "_sr, m_matchers, std::index_sequence_for<MatcherTs...>{});
            }

            std::array<void const*, sizeof...(MatcherTs)> m_matchers;

            //! Avoids type nesting for `GenericAllOf && GenericAllOf` case
            template<typename... MatchersRHS>
            friend
            MatchAllOfGeneric<MatcherTs..., MatchersRHS...> operator && (
                    MatchAllOfGeneric<MatcherTs...>&& lhs,
                    MatchAllOfGeneric<MatchersRHS...>&& rhs) {
                return MatchAllOfGeneric<MatcherTs..., MatchersRHS...>{array_cat(std::move(lhs.m_matchers), std::move(rhs.m_matchers))};
            }

            //! Avoids type nesting for `GenericAllOf && some matcher` case
            template<typename MatcherRHS>
            friend std::enable_if_t<is_matcher<MatcherRHS>::value,
            MatchAllOfGeneric<MatcherTs..., MatcherRHS>> operator && (
                    MatchAllOfGeneric<MatcherTs...>&& lhs,
                    MatcherRHS const& rhs) {
                return MatchAllOfGeneric<MatcherTs..., MatcherRHS>{array_cat(std::move(lhs.m_matchers), static_cast<void const*>(&rhs))};
            }

            //! Avoids type nesting for `some matcher && GenericAllOf` case
            template<typename MatcherLHS>
            friend std::enable_if_t<is_matcher<MatcherLHS>::value,
            MatchAllOfGeneric<MatcherLHS, MatcherTs...>> operator && (
                    MatcherLHS const& lhs,
                    MatchAllOfGeneric<MatcherTs...>&& rhs) {
                return MatchAllOfGeneric<MatcherLHS, MatcherTs...>{array_cat(static_cast<void const*>(std::addressof(lhs)), std::move(rhs.m_matchers))};
            }
        };


        template<typename... MatcherTs>
        struct MatchAnyOfGeneric final : MatcherGenericBase {
            MatchAnyOfGeneric(MatchAnyOfGeneric const&) = delete;
            MatchAnyOfGeneric& operator=(MatchAnyOfGeneric const&) = delete;
            MatchAnyOfGeneric(MatchAnyOfGeneric&&) = default;
            MatchAnyOfGeneric& operator=(MatchAnyOfGeneric&&) = default;

            MatchAnyOfGeneric(MatcherTs const&... matchers) : m_matchers{ {std::addressof(matchers)...} } {}
            explicit MatchAnyOfGeneric(std::array<void const*, sizeof...(MatcherTs)> matchers) : m_matchers{matchers} {}

            template<typename Arg>
            bool match(Arg&& arg) const {
                return match_any_of<MatcherTs...>(arg, m_matchers, std::index_sequence_for<MatcherTs...>{});
            }

            std::string describe() const override {
                return describe_multi_matcher<MatcherTs...>(" or "_sr, m_matchers, std::index_sequence_for<MatcherTs...>{});
            }

            std::array<void const*, sizeof...(MatcherTs)> m_matchers;

            //! Avoids type nesting for `GenericAnyOf || GenericAnyOf` case
            template<typename... MatchersRHS>
            friend MatchAnyOfGeneric<MatcherTs..., MatchersRHS...> operator || (
                    MatchAnyOfGeneric<MatcherTs...>&& lhs,
                    MatchAnyOfGeneric<MatchersRHS...>&& rhs) {
                return MatchAnyOfGeneric<MatcherTs..., MatchersRHS...>{array_cat(std::move(lhs.m_matchers), std::move(rhs.m_matchers))};
            }

            //! Avoids type nesting for `GenericAnyOf || some matcher` case
            template<typename MatcherRHS>
            friend std::enable_if_t<is_matcher<MatcherRHS>::value,
            MatchAnyOfGeneric<MatcherTs..., MatcherRHS>> operator || (
                    MatchAnyOfGeneric<MatcherTs...>&& lhs,
                    MatcherRHS const& rhs) {
                return MatchAnyOfGeneric<MatcherTs..., MatcherRHS>{array_cat(std::move(lhs.m_matchers), static_cast<void const*>(std::addressof(rhs)))};
            }

            //! Avoids type nesting for `some matcher || GenericAnyOf` case
            template<typename MatcherLHS>
            friend std::enable_if_t<is_matcher<MatcherLHS>::value,
            MatchAnyOfGeneric<MatcherLHS, MatcherTs...>> operator || (
                MatcherLHS const& lhs,
                MatchAnyOfGeneric<MatcherTs...>&& rhs) {
                return MatchAnyOfGeneric<MatcherLHS, MatcherTs...>{array_cat(static_cast<void const*>(std::addressof(lhs)), std::move(rhs.m_matchers))};
            }
        };


        template<typename MatcherT>
        struct MatchNotOfGeneric final : MatcherGenericBase {
            MatchNotOfGeneric(MatchNotOfGeneric const&) = delete;
            MatchNotOfGeneric& operator=(MatchNotOfGeneric const&) = delete;
            MatchNotOfGeneric(MatchNotOfGeneric&&) = default;
            MatchNotOfGeneric& operator=(MatchNotOfGeneric&&) = default;

            explicit MatchNotOfGeneric(MatcherT const& matcher) : m_matcher{matcher} {}

            template<typename Arg>
            bool match(Arg&& arg) const {
                return !m_matcher.match(arg);
            }

            std::string describe() const override {
                return "not " + m_matcher.toString();
            }

            //! Negating negation can just unwrap and return underlying matcher
            friend MatcherT const& operator ! (MatchNotOfGeneric<MatcherT> const& matcher) {
                return matcher.m_matcher;
            }
        private:
            MatcherT const& m_matcher;
        };
    } // namespace Detail


    // compose only generic matchers
    template<typename MatcherLHS, typename MatcherRHS>
    std::enable_if_t<Detail::are_generic_matchers<MatcherLHS, MatcherRHS>::value, Detail::MatchAllOfGeneric<MatcherLHS, MatcherRHS>>
        operator && (MatcherLHS const& lhs, MatcherRHS const& rhs) {
        return { lhs, rhs };
    }

    template<typename MatcherLHS, typename MatcherRHS>
    std::enable_if_t<Detail::are_generic_matchers<MatcherLHS, MatcherRHS>::value, Detail::MatchAnyOfGeneric<MatcherLHS, MatcherRHS>>
        operator || (MatcherLHS const& lhs, MatcherRHS const& rhs) {
        return { lhs, rhs };
    }

    //! Wrap provided generic matcher in generic negator
    template<typename MatcherT>
    std::enable_if_t<Detail::is_generic_matcher<MatcherT>::value, Detail::MatchNotOfGeneric<MatcherT>>
        operator ! (MatcherT const& matcher) {
        return Detail::MatchNotOfGeneric<MatcherT>{matcher};
    }


    // compose mixed generic and non-generic matchers
    template<typename MatcherLHS, typename ArgRHS>
    std::enable_if_t<Detail::is_generic_matcher<MatcherLHS>::value, Detail::MatchAllOfGeneric<MatcherLHS, MatcherBase<ArgRHS>>>
        operator && (MatcherLHS const& lhs, MatcherBase<ArgRHS> const& rhs) {
        return { lhs, rhs };
    }

    template<typename ArgLHS, typename MatcherRHS>
    std::enable_if_t<Detail::is_generic_matcher<MatcherRHS>::value, Detail::MatchAllOfGeneric<MatcherBase<ArgLHS>, MatcherRHS>>
        operator && (MatcherBase<ArgLHS> const& lhs, MatcherRHS const& rhs) {
        return { lhs, rhs };
    }

    template<typename MatcherLHS, typename ArgRHS>
    std::enable_if_t<Detail::is_generic_matcher<MatcherLHS>::value, Detail::MatchAnyOfGeneric<MatcherLHS, MatcherBase<ArgRHS>>>
        operator || (MatcherLHS const& lhs, MatcherBase<ArgRHS> const& rhs) {
        return { lhs, rhs };
    }

    template<typename ArgLHS, typename MatcherRHS>
    std::enable_if_t<Detail::is_generic_matcher<MatcherRHS>::value, Detail::MatchAnyOfGeneric<MatcherBase<ArgLHS>, MatcherRHS>>
        operator || (MatcherBase<ArgLHS> const& lhs, MatcherRHS const& rhs) {
        return { lhs, rhs };
    }

} // namespace Matchers
} // namespace Catch

#endif //TWOBLUECUBES_CATCH_MATCHERS_TEMPLATES_HPP_INCLUDED
