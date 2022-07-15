/**
 * @file ukv_leveldb.cpp
 * @author Ashot Vardanian
 *
 * @brief LevelDB is a fast key-value storage library written at Google that provides an ordered mapping from string
 * keys to string values
 */

#include <leveldb/db.h>
#include <leveldb/comparator.h>
#include <leveldb/write_batch.h>

#include "ukv/ukv.h"
#include "helpers.hpp"

using namespace unum::ukv;
using namespace unum;

/*********************************************************/
/*****************   Structures & Consts  ****************/
/*********************************************************/

ukv_collection_t ukv_default_collection_k = NULL;
ukv_val_len_t ukv_val_len_missing_k = std::numeric_limits<ukv_val_len_t>::max();
ukv_key_t ukv_key_unknown_k = std::numeric_limits<ukv_key_t>::max();

using level_db_t = leveldb::DB;
using level_status_t = leveldb::Status;
using level_options_t = leveldb::Options;
using value_uptr_t = std::unique_ptr<std::string>;

struct key_comparator_t final : public leveldb::Comparator {

    inline int Compare(leveldb::Slice const& a, leveldb::Slice const& b) const override {
        auto ai = *reinterpret_cast<ukv_key_t const*>(a.data());
        auto bi = *reinterpret_cast<ukv_key_t const*>(b.data());
        if (ai == bi)
            return 0;
        return ai < bi ? -1 : 1;
    }

    char const* Name() const override { return "Integral"; }

    void FindShortestSeparator(std::string* start, leveldb::Slice const& limit) const override {}

    void FindShortSuccessor(std::string* key) const override {
        auto& int_key = *reinterpret_cast<ukv_key_t*>(key->data());
        ++int_key;
    }
};

static key_comparator_t key_comparator_k = {};

/*********************************************************/
/*****************	 C++ Implementation	  ****************/
/*********************************************************/

inline leveldb::Slice to_slice(ukv_key_t const& key) noexcept {
    return {reinterpret_cast<char const*>(&key), sizeof(ukv_key_t)};
}

inline leveldb::Slice to_slice(value_view_t value) noexcept {
    return {reinterpret_cast<const char*>(value.begin()), value.size()};
}

inline value_uptr_t make_value(ukv_error_t* c_error) noexcept {
    value_uptr_t value_uptr;
    try {
        value_uptr = std::make_unique<std::string>();
    }
    catch (...) {
        *c_error = "Fail to allocate value";
    }
    return value_uptr;
}

bool export_error(level_status_t const& status, ukv_error_t* c_error) {
    if (status.ok())
        return false;

    if (status.IsCorruption())
        *c_error = "Failure: DB Corrpution";
    else if (status.IsIOError())
        *c_error = "Failure: IO  Error";
    else if (status.IsInvalidArgument())
        *c_error = "Failure: Invalid Argument";
    else
        *c_error = "Failure";
    return true;
}

void ukv_open(char const* c_config, ukv_t* c_db, ukv_error_t* c_error) {
    level_db_t* db_ptr = nullptr;
    level_options_t options;
    options.create_if_missing = true;
    options.comparator = &key_comparator_k;
    level_status_t status = level_db_t::Open(options, "./tmp/leveldb/", &db_ptr);
    if (!status.ok()) {
        *c_error = "Couldn't open LevelDB";
        return;
    }
    *c_db = db_ptr;
}

void write_one( //
    level_db_t& db,
    write_tasks_soa_t const& tasks,
    ukv_size_t const,
    leveldb::WriteOptions const& options,
    ukv_error_t* c_error) {

    auto task = tasks[0];
    auto key = to_slice(task.key);
    level_status_t status = task.is_deleted() ? db.Delete(options, key) : db.Put(options, key, to_slice(task.view()));
    export_error(status, c_error);
}

void write_many( //
    level_db_t& db,
    write_tasks_soa_t const& tasks,
    ukv_size_t const n,
    leveldb::WriteOptions const& options,
    ukv_error_t* c_error) {

    leveldb::WriteBatch batch;
    for (ukv_size_t i = 0; i != n; ++i) {
        auto task = tasks[i];
        auto key = to_slice(task.key);
        if (task.is_deleted())
            batch.Delete(key);
        else
            batch.Put(key, to_slice(task.view()));
    }

    level_status_t status = db.Write(options, &batch);
    export_error(status, c_error);
}

void ukv_write( //
    ukv_t const c_db,
    ukv_txn_t const c_txn,

    ukv_collection_t const* c_cols,
    ukv_size_t const c_cols_stride,

    ukv_key_t const* c_keys,
    ukv_size_t const c_keys_count,
    ukv_size_t const c_keys_stride,

    ukv_val_ptr_t const* c_vals,
    ukv_size_t const c_vals_stride,

    ukv_val_len_t const* c_offs,
    ukv_size_t const c_offs_stride,

    ukv_val_len_t const* c_lens,
    ukv_size_t const c_lens_stride,

    ukv_options_t const c_options,
    ukv_arena_t*,
    ukv_error_t* c_error) {

    level_db_t& db = *reinterpret_cast<level_db_t*>(c_db);
    strided_iterator_gt<ukv_collection_t const> cols {c_cols, c_cols_stride};
    strided_iterator_gt<ukv_key_t const> keys {c_keys, c_keys_stride};
    strided_iterator_gt<ukv_val_ptr_t const> vals {c_vals, c_vals_stride};
    strided_iterator_gt<ukv_val_len_t const> offs {c_offs, c_offs_stride};
    strided_iterator_gt<ukv_val_len_t const> lens {c_lens, c_lens_stride};
    write_tasks_soa_t tasks {cols, keys, vals, offs, lens};

    leveldb::WriteOptions options;
    if (c_options & ukv_option_write_flush_k)
        options.sync = true;

    try {
        auto func = c_keys_count == 1 ? &write_one : &write_many;
        func(db, tasks, c_keys_count, options, c_error);
    }
    catch (...) {
        *c_error = "Write Failure";
    }
}

void read_one( //
    level_db_t& db,
    read_tasks_soa_t const& tasks,
    ukv_size_t const,
    leveldb::ReadOptions const& options,
    std::string& value,
    ukv_val_len_t** c_found_lengths,
    ukv_val_ptr_t* c_found_values,
    stl_arena_t& arena,
    ukv_error_t* c_error) {

    read_task_t task = tasks[0];
    level_status_t status = db.Get(options, to_slice(task.key), &value);
    if (!status.IsNotFound())
        if (export_error(status, c_error))
            return;

    auto bytes_in_value = static_cast<ukv_size_t>(value.size());
    auto exported_len = status.IsNotFound() ? ukv_val_len_missing_k : bytes_in_value;
    auto tape = prepare_memory(arena.output_tape, sizeof(ukv_size_t) + bytes_in_value, c_error);
    if (*c_error)
        return;

    std::memcpy(tape, &exported_len, sizeof(ukv_size_t));
    std::memcpy(tape + sizeof(ukv_size_t), value.data(), bytes_in_value);

    *c_found_lengths = reinterpret_cast<ukv_val_len_t*>(tape);
    *c_found_values = reinterpret_cast<ukv_val_ptr_t>(tape + sizeof(ukv_size_t));
}

void read_many( //
    level_db_t& db,
    read_tasks_soa_t const& tasks,
    ukv_size_t const n,
    leveldb::ReadOptions const& options,
    std::string& value,
    ukv_val_len_t** c_found_lengths,
    ukv_val_ptr_t* c_found_values,
    stl_arena_t& arena,
    ukv_error_t* c_error) {

    ukv_size_t lens_bytes = sizeof(ukv_val_len_t) * n;
    byte_t* tape = prepare_memory(arena.output_tape, lens_bytes, c_error);
    if (*c_error)
        return;

    ukv_val_len_t* lens = reinterpret_cast<ukv_val_len_t*>(tape);
    std::fill_n(lens, n, 0);

    for (ukv_size_t i = 0; i != n; ++i) {
        read_task_t task = tasks[i];
        level_status_t status = db.Get(options, to_slice(task.key), &value);
        if (status.IsNotFound())
            continue;
        if (export_error(status, c_error))
            return;

        auto old_tape_len = arena.output_tape.size();
        auto bytes_in_value = value.size();
        tape = prepare_memory(arena.output_tape, old_tape_len + bytes_in_value, c_error);
        lens = reinterpret_cast<ukv_val_len_t*>(tape);
        if (*c_error)
            return;

        std::memcpy(tape + old_tape_len, value.data(), bytes_in_value);
        lens[i] = static_cast<ukv_val_len_t>(bytes_in_value);
    }
}

void ukv_read( //
    ukv_t const c_db,
    ukv_txn_t const,

    ukv_collection_t const*,
    ukv_size_t const,

    ukv_key_t const* c_keys,
    ukv_size_t const c_keys_count,
    ukv_size_t const c_keys_stride,

    ukv_options_t const,

    ukv_val_len_t** c_found_lengths,
    ukv_val_ptr_t* c_found_values,

    ukv_arena_t* c_arena,
    ukv_error_t* c_error) {

    level_db_t& db = *reinterpret_cast<level_db_t*>(c_db);

    leveldb::ReadOptions options;
    stl_arena_t& arena = *cast_arena(c_arena, c_error);
    strided_iterator_gt<ukv_key_t const> keys {c_keys, c_keys_stride};
    read_tasks_soa_t tasks {{}, keys};

    auto value_uptr = make_value(c_error);
    std::string& value = *value_uptr.get();

    try {
        auto func = c_keys_count == 1 ? &read_one : &read_many;
        func(db, tasks, c_keys_count, options, value, c_found_lengths, c_found_values, arena, c_error);
    }
    catch (...) {
        *c_error = "Read Failure";
    }
}

// void ukv_scan( //
//     ukv_t const c_db,
//     ukv_txn_t const,

//     ukv_collection_t const*,
//     ukv_size_t const,

//     ukv_key_t const* c_min_keys,
//     ukv_size_t const c_min_keys_count,
//     ukv_size_t const c_min_keys_stride,

//     ukv_size_t const* c_scan_lengths,
//     ukv_size_t const c_scan_lengths_stride,

//     ukv_options_t const,

//     ukv_key_t** c_found_keys,
//     ukv_val_len_t** c_found_lengths,

//     ukv_arena_t* c_arena,
//     ukv_error_t* c_error) {

//     stl_arena_t& arena = *cast_arena(c_arena, c_error);
//     if (*c_error)
//         return;

//     level_db_t& db = *reinterpret_cast<level_db_t*>(c_db);
//     strided_iterator_gt<ukv_key_t const> keys {c_min_keys, c_min_keys_stride};
//     strided_iterator_gt<ukv_size_t const> lens {c_scan_lengths, c_scan_lengths_stride};
//     scan_tasks_soa_t tasks {{}, keys, lens};
// }

void ukv_collection_open( //
    ukv_t const,
    ukv_str_view_t,
    ukv_str_view_t,
    ukv_collection_t*,
    ukv_error_t* c_error) {
    *c_error = "Collections not supported by LevelDB!";
}

void ukv_collection_remove( //
    ukv_t const,
    ukv_str_view_t,
    ukv_error_t* c_error) {
    *c_error = "Collections not supported by LevelDB!";
}

void ukv_control( //
    ukv_t const,
    ukv_str_view_t,
    ukv_str_view_t*,
    ukv_error_t* c_error) {
    *c_error = "Controls not supported by LevelDB!";
}

void ukv_txn_begin( //
    ukv_t const,
    ukv_size_t const,
    ukv_options_t const,
    ukv_txn_t*,
    ukv_error_t* c_error) {
    *c_error = "Transactions not supported by LevelDB!";
}

void ukv_txn_commit( //
    ukv_txn_t const,
    ukv_options_t const,
    ukv_error_t* c_error) {
    *c_error = "Transactions not supported by LevelDB!";
}

void ukv_arena_free(ukv_t const, ukv_arena_t c_arena) {
    if (!c_arena)
        return;
    stl_arena_t& arena = *reinterpret_cast<stl_arena_t*>(c_arena);
    delete &arena;
}

void ukv_txn_free(ukv_t const, ukv_txn_t) {
}

void ukv_collection_free(ukv_t const, ukv_collection_t const) {
}

void ukv_free(ukv_t c_db) {
    if (!c_db)
        return;
    level_db_t* db = reinterpret_cast<level_db_t*>(c_db);
    delete db;
}

void ukv_error_free(ukv_error_t const) {
}