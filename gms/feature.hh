/*
 * Copyright (C) 2016 ScyllaDB
 */

/*
 * This file is part of Scylla.
 *
 * Scylla is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Scylla is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Scylla.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <boost/signals2.hpp>
#include <boost/signals2/dummy_mutex.hpp>

#include <seastar/core/shared_future.hh>
#include <seastar/util/noncopyable_function.hh>

using namespace seastar;

namespace bs2 = boost::signals2;

namespace gms {

class feature_service;

/**
 * A gossip feature tracks whether all the nodes the current one is
 * aware of support the specified feature.
 *
 * A feature should only be created once the gossiper is available.
 */
class feature final {
    using signal_type = bs2::signal_type<void (), bs2::keywords::mutex_type<bs2::dummy_mutex>>::type;

    feature_service* _service = nullptr;
    sstring _name;
    bool _enabled = false;
    mutable shared_promise<> _pr;
    mutable signal_type _s;
    friend class gossiper;
public:
    using listener_registration = std::any;
    class listener {
        friend class feature;
        bs2::scoped_connection _conn;
        signal_type::slot_type _slot;
        const signal_type::slot_type& get_slot() const { return _slot; }
        void set_connection(bs2::scoped_connection&& conn) { _conn = std::move(conn); }
        void callback() {
            _conn.disconnect();
            on_enabled();
        }
    protected:
        bool _started = false;
    public:
        listener() : _slot(signal_type::slot_type(&listener::callback, this)) {}
        listener(const listener&) = delete;
        listener(listener&&) = delete;
        listener& operator=(const listener&) = delete;
        listener& operator=(listener&&) = delete;
        // Has to run inside seastar::async context
        virtual void on_enabled() = 0;
    };
    explicit feature(feature_service& service, sstring name, bool enabled = false);
    feature() = default;
    ~feature();
    feature(const feature& other) = delete;
    // Has to run inside seastar::async context
    void enable();
    feature& operator=(feature&& other);
    const sstring& name() const {
        return _name;
    }
    explicit operator bool() const {
        return _enabled;
    }
    friend inline std::ostream& operator<<(std::ostream& os, const feature& f) {
        return os << "{ gossip feature = " << f._name << " }";
    }
    future<> when_enabled() const { return _pr.get_shared_future(); }
    void when_enabled(listener& callback) const {
        callback.set_connection(_s.connect(callback.get_slot()));
        if (_enabled) {
            _s();
        }
    }
    // Will call the callback functor when this feature is enabled, unless
    // the returned listener_registration is destroyed earlier.
    listener_registration when_enabled(seastar::noncopyable_function<void()> callback) const {
        struct wrapper : public listener {
            seastar::noncopyable_function<void()> _func;
            wrapper(seastar::noncopyable_function<void()> func) : _func(std::move(func)) {}
            void on_enabled() override { _func(); }
        };
        auto holder = make_lw_shared<wrapper>(std::move(callback));
        when_enabled(*holder);
        return holder;
    }
};

namespace features {

static const sstring RANGE_TOMBSTONES = "RANGE_TOMBSTONES";
static const sstring LARGE_PARTITIONS = "LARGE_PARTITIONS";
static const sstring MATERIALIZED_VIEWS = "MATERIALIZED_VIEWS";
static const sstring COUNTERS = "COUNTERS";
static const sstring INDEXES = "INDEXES";
static const sstring DIGEST_MULTIPARTITION_READ = "DIGEST_MULTIPARTITION_READ";
static const sstring CORRECT_COUNTER_ORDER = "CORRECT_COUNTER_ORDER";
static const sstring SCHEMA_TABLES_V3 = "SCHEMA_TABLES_V3";
static const sstring CORRECT_NON_COMPOUND_RANGE_TOMBSTONES = "CORRECT_NON_COMPOUND_RANGE_TOMBSTONES";
static const sstring WRITE_FAILURE_REPLY = "WRITE_FAILURE_REPLY";
static const sstring XXHASH = "XXHASH";
static const sstring UDF = "UDF";
static const sstring ROLES = "ROLES";
static const sstring LA_SSTABLE = "LA_SSTABLE_FORMAT";
static const sstring STREAM_WITH_RPC_STREAM = "STREAM_WITH_RPC_STREAM";
static const sstring MC_SSTABLE = "MC_SSTABLE_FORMAT";
static const sstring ROW_LEVEL_REPAIR = "ROW_LEVEL_REPAIR";
static const sstring TRUNCATION_TABLE = "TRUNCATION_TABLE";
static const sstring CORRECT_STATIC_COMPACT_IN_MC = "CORRECT_STATIC_COMPACT_IN_MC";
static const sstring UNBOUNDED_RANGE_TOMBSTONES = "UNBOUNDED_RANGE_TOMBSTONES";
static const sstring VIEW_VIRTUAL_COLUMNS = "VIEW_VIRTUAL_COLUMNS";
static const sstring DIGEST_INSENSITIVE_TO_EXPIRY = "DIGEST_INSENSITIVE_TO_EXPIRY";
static const sstring COMPUTED_COLUMNS = "COMPUTED_COLUMNS";
static const sstring CDC = "CDC";
static const sstring NONFROZEN_UDTS = "NONFROZEN_UDTS";
static const sstring HINTED_HANDOFF_SEPARATE_CONNECTION = "HINTED_HANDOFF_SEPARATE_CONNECTION";
static const sstring LWT = "LWT";

}

} // namespace gms
