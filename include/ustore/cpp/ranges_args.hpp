/**
 * @file ranges_args.hpp
 * @author Ashot Vardanian
 * @date 4 Jul 2022
 * @addtogroup Cpp
 *
 * @brief Range-like argument resolvers for UStore.
 */

#pragma once
#include <limits>                // `std::numeric_limits`

#include "ustore/cpp/ranges.hpp" // `strided_iterator_gt`
#include "ustore/cpp/status.hpp" // `return_error_if_m`

namespace unum::ustore {

using keys_view_t = strided_range_gt<ustore_key_t const>;
using fields_view_t = strided_range_gt<ustore_str_view_t const>;

struct place_t {
    ustore_collection_t collection;
    ustore_key_t const& key;
    ustore_str_view_t field;

    inline collection_key_t collection_key() const noexcept { return {collection, key}; }
    inline collection_key_field_t collection_key_field() const noexcept { return {collection, key, field}; }
};
/**
 * Working with batched data is ugly in C++.
 * This handle doesn't help in the general case,
 * but at least allow reusing the arguments.
 */
struct places_arg_t {
    using value_type = place_t;
    strided_iterator_gt<ustore_collection_t const> collections_begin;
    strided_iterator_gt<ustore_key_t const> keys_begin;
    strided_iterator_gt<ustore_str_view_t const> fields_begin;
    ustore_size_t count {0};

    inline std::size_t size() const noexcept { return count; }
    inline place_t operator[](std::size_t i) const noexcept {
        ustore_collection_t collection = collections_begin ? collections_begin[i] : ustore_collection_main_k;
        ustore_key_t const& key = keys_begin[i];
        ustore_str_view_t field = fields_begin ? fields_begin[i] : nullptr;
        return {collection, key, field};
    }

    bool same_collection() const noexcept {
        return strided_range_gt<ustore_collection_t const>(collections_begin, count).same_elements();
    }
};

/**
 * Working with batched data is ugly in C++.
 * This handle doesn't help in the general case,
 * but at least allow reusing the arguments.
 */
struct contents_arg_t {
    using value_type = value_view_t;
    bits_view_t presences_begin;
    strided_iterator_gt<ustore_length_t const> offsets_begin;
    strided_iterator_gt<ustore_length_t const> lengths_begin;
    strided_iterator_gt<ustore_bytes_cptr_t const> contents_begin;
    ustore_size_t count {0};
    ustore_char_t separator {'\0'};

    inline std::size_t size() const noexcept { return count; }
    inline value_view_t operator[](std::size_t i) const noexcept {
        if (!contents_begin || !contents_begin[i] || (presences_begin && !presences_begin[i]))
            return {};

        auto begin = reinterpret_cast<byte_t const*>(contents_begin[i]);
        auto off = offsets_begin ? offsets_begin[i] : 0u;
        ustore_length_t len = 0;
        if (lengths_begin)
            len = lengths_begin[i];
        else if (offsets_begin)
            len = offsets_begin[i + 1] - off;
        else {
            auto item = reinterpret_cast<char const*>(begin) + off;
            while (item[len++] != separator)
                ;
            --len;
        }
        return {begin + off, len};
    }

    inline bool is_arrow() const noexcept { return contents_begin.repeats() && offsets_begin && !lengths_begin; }

    inline bool is_continuous() const noexcept {
        auto last = operator[](0);
        for (std::size_t i = 1; i != size(); ++i) {
            auto value = operator[](i);
            if (value.begin() != last.end())
                return false;
            last = value;
        }
        return true;
    }
};

struct scan_t {
    ustore_collection_t collection;
    ustore_key_t min_key;
    ustore_length_t limit;
};

/**
 * @brief Arguments of `ustore_scan()` aggregated into a Structure-of-Arrays.
 * Is used to validate various combinations of arguments, strides, NULLs, etc.
 */
struct scans_arg_t {
    strided_iterator_gt<ustore_collection_t const> collections;
    strided_iterator_gt<ustore_key_t const> start_keys;
    strided_iterator_gt<ustore_length_t const> limits;
    ustore_size_t count = 0;

    inline std::size_t size() const noexcept { return count; }
    inline scan_t operator[](std::size_t i) const noexcept {
        ustore_collection_t collection = collections ? collections[i] : ustore_collection_main_k;
        ustore_key_t min_key = start_keys ? start_keys[i] : std::numeric_limits<ustore_key_t>::min();
        ustore_length_t limit = limits[i];
        return {collection, min_key, limit};
    }

    bool same_collection() const noexcept {
        strided_range_gt<ustore_collection_t const> range(collections, count);
        return range.same_elements();
    }
};

struct sample_arg_t {
    ustore_collection_t collection;
    ustore_length_t limit;
};

/**
 * @brief Arguments of `ustore_sample` aggregated into a Structure-of-Arrays.
 * Is used to validate various combinations of arguments, strides, NULLs, etc.
 */
struct sample_args_t {
    strided_iterator_gt<ustore_collection_t const> collections;
    strided_iterator_gt<ustore_length_t const> limits;
    ustore_size_t count = 0;

    inline std::size_t size() const noexcept { return count; }
    inline sample_arg_t operator[](std::size_t i) const noexcept {
        ustore_collection_t collection = collections ? collections[i] : ustore_collection_main_k;
        ustore_length_t limit = limits[i];
        return {collection, limit};
    }

    bool same_collection() const noexcept {
        strided_range_gt<ustore_collection_t const> range(collections, count);
        return range.same_elements();
    }
};

struct find_edge_t {
    ustore_collection_t collection;
    ustore_key_t const& vertex_id;
    ustore_vertex_role_t role;
};
/**
 * Working with batched data is ugly in C++.
 * This handle doesn't help in the general case,
 * but at least allow reusing the arguments.
 */
struct find_edges_t {
    using value_type = find_edge_t;
    strided_iterator_gt<ustore_collection_t const> collections_begin;
    strided_iterator_gt<ustore_key_t const> vertex_id_begin;
    strided_iterator_gt<ustore_vertex_role_t const> roles_begin;
    ustore_size_t count = 0;

    inline std::size_t size() const noexcept { return count; }
    inline find_edge_t operator[](std::size_t i) const noexcept {
        ustore_collection_t collection = collections_begin ? collections_begin[i] : ustore_collection_main_k;
        ustore_key_t const& vertex_id = vertex_id_begin[i];
        ustore_vertex_role_t role = roles_begin ? roles_begin[i] : ustore_vertex_role_any_k;
        return {collection, vertex_id, role};
    }

    bool same_collection() const noexcept {
        return strided_range_gt<ustore_collection_t const>(collections_begin, count).same_elements();
    }
};

template <typename id_at>
struct edges_range_gt {

    using id_t = id_at;
    using tuple_t = std::conditional_t<std::is_const_v<id_t>, edge_t const, edge_t>;
    static_assert(sizeof(tuple_t) == 3 * sizeof(id_t));

    strided_range_gt<id_t> source_ids;
    strided_range_gt<id_t> target_ids;
    strided_range_gt<id_t> edge_ids;

    inline edges_range_gt() = default;
    inline edges_range_gt(edges_range_gt&&) = default;
    inline edges_range_gt(edges_range_gt const&) = default;
    inline edges_range_gt& operator=(edges_range_gt&&) = default;
    inline edges_range_gt& operator=(edges_range_gt const&) = default;

    inline edges_range_gt(strided_range_gt<id_t> sources,
                          strided_range_gt<id_t> targets,
                          strided_range_gt<id_t> edges = {&ustore_default_edge_id_k, 1}) noexcept
        : source_ids(sources), target_ids(targets), edge_ids(edges) {}

    inline edges_range_gt(tuple_t* ptr, tuple_t* end) noexcept {
        auto strided = strided_range_gt<tuple_t>({ptr, sizeof(tuple_t)}, end - ptr);
        source_ids = strided.members(&edge_t::source_id);
        target_ids = strided.members(&edge_t::target_id);
        edge_ids = strided.members(&edge_t::id);
    }

    inline std::size_t size() const noexcept { return std::min(source_ids.count(), target_ids.count()); }

    inline edge_t operator[](std::size_t i) const noexcept {
        edge_t result;
        result.source_id = source_ids[i];
        result.target_id = target_ids[i];
        result.id = edge_ids[i];
        return result;
    }

    inline operator edges_range_gt<id_at const>() const noexcept { return immutable(); }
    inline edges_range_gt<id_at const> immutable() const noexcept {
        return {source_ids.immutable(), target_ids.immutable(), edge_ids.immutable()};
    }
};

using edges_span_t = edges_range_gt<ustore_key_t>;
using edges_view_t = edges_range_gt<ustore_key_t const>;

template <typename tuples_at>
auto edges(tuples_at&& tuples) noexcept {
    using tuples_t = std::remove_reference_t<tuples_at>;
    using element_t = typename tuples_t::value_type;
    constexpr bool immutable_k = std::is_const_v<element_t> || std::is_const_v<tuples_t>;
    using result_t = std::conditional_t<immutable_k, edges_view_t, edges_span_t>;
    auto ptr = std::data(tuples);
    auto count = std::size(tuples);
    return result_t(ptr, ptr + count);
}

inline bool same_collections_are_named(strided_iterator_gt<ustore_collection_t const> collections_begin) noexcept {
    return collections_begin && collections_begin[0] != ustore_collection_main_k;
}

template <typename enum_at, typename allowed_mask_at>
inline bool enum_is_subset(enum_at enum_value, allowed_mask_at allowed) noexcept {
    return (enum_value & ~allowed) == 0;
}

inline void validate_write(ustore_transaction_t const c_txn,
                           places_arg_t const& places,
                           contents_arg_t const& contents,
                           ustore_options_t const c_options,
                           ustore_error_t* c_error) noexcept {

    auto allowed_options =                       //
        ustore_option_transaction_dont_watch_k | //
        ustore_option_dont_discard_memory_k |    //
        ustore_option_write_flush_k;
    return_error_if_m(enum_is_subset(c_options, allowed_options), c_error, args_wrong_k, "Invalid options!");

    return_error_if_m(places.keys_begin, c_error, args_wrong_k, "No keys were provided!");

    bool const remove_all = !contents.contents_begin;
    if (remove_all)
        return_error_if_m(!contents.lengths_begin && !contents.offsets_begin,
                          c_error,
                          args_wrong_k,
                          "Can't address NULLs!");
}

inline void validate_read(ustore_transaction_t const c_txn,
                          places_arg_t const& places,
                          ustore_options_t const c_options,
                          ustore_error_t* c_error) noexcept {

    auto allowed_options =                       //
        ustore_option_transaction_dont_watch_k | //
        ustore_option_dont_discard_memory_k |    //
        ustore_option_read_shared_memory_k;
    return_error_if_m(enum_is_subset(c_options, allowed_options), c_error, args_wrong_k, "Invalid options!");

    return_error_if_m(places.keys_begin, c_error, args_wrong_k, "No keys were provided!");
}

inline void validate_scan(ustore_transaction_t const c_txn,
                          scans_arg_t const& args,
                          ustore_options_t const c_options,
                          ustore_error_t* c_error) noexcept {

    auto allowed_options =                       //
        ustore_option_transaction_dont_watch_k | //
        ustore_option_dont_discard_memory_k |    //
        ustore_option_read_shared_memory_k |     //
        ustore_option_scan_bulk_k;
    return_error_if_m(enum_is_subset(c_options, allowed_options), c_error, args_wrong_k, "Invalid options!");

    return_error_if_m(args.limits, c_error, args_wrong_k, "Full scans aren't supported - paginate!");
}

inline void validate_transaction_begin(ustore_transaction_t const c_txn,
                                       ustore_options_t const c_options,
                                       ustore_error_t* c_error) noexcept {

    return_error_if_m(c_txn, c_error, args_wrong_k, "Transaction is uninitialized");
    return_error_if_m(enum_is_subset(c_options, ustore_option_transaction_dont_watch_k),
                      c_error,
                      args_wrong_k,
                      "Invalid options!");
}

inline void validate_transaction_commit(ustore_transaction_t const c_txn,
                                        ustore_options_t const c_options,
                                        ustore_error_t* c_error) noexcept {

    return_error_if_m(c_txn, c_error, args_wrong_k, "Transaction is uninitialized");
    return_error_if_m(enum_is_subset(c_options, ustore_option_write_flush_k),
                      c_error,
                      args_wrong_k,
                      "Invalid options!");
}

} // namespace unum::ustore
