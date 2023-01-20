/*******************************************************************************
 * Copyright 2023 MINRES Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************/

#include "lwtr.h"

#include <unordered_map>
#include <sstream>


namespace lwtr {
///////////////////////////////////////////////////////////////////////////////
/// tx_db
///////////////////////////////////////////////////////////////////////////////

struct tx_db::impl {
	const std::string file_name;
	tx_relation_handle relation_handle_counter{0};
    std::unordered_map<tx_relation_handle, std::string> relation_by_handle_map;
    std::unordered_map<std::string, tx_relation_handle> relation_by_name_map;

    impl(std::string const& s):file_name(s){}

    static tx_db* default_db;
	static std::vector<std::pair<uint64_t, tx_db_class_cb>> cb;
	using cb_entry = std::vector<std::pair<uint64_t, tx_db_class_cb>>::value_type;
};
tx_db* tx_db::impl::default_db=nullptr;
std::vector<std::pair<uint64_t, tx_db::tx_db_class_cb>> tx_db::impl::cb;

tx_db::tx_db(std::string const& recording_file_name, sc_core::sc_time_unit)
: pimpl(new tx_db::impl(recording_file_name))
{
	impl::default_db=this;
	for(auto& e:impl::cb) e.second(*this, CREATE);
}

tx_db::~tx_db() {
	for(auto& e:impl::cb) e.second(*this, DELETE);
}

void tx_db::set_default_db(tx_db* db) {
	impl::default_db=db;
}

tx_db* tx_db::get_default_db() {
	return impl::default_db;
}

uint64_t tx_db::register_class_cb(tx_db_class_cb cb) {
	auto index = impl::cb.size()?impl::cb.back().first+1:0;
	impl::cb.push_back(std::make_pair(index, cb));
	return index;
}

void tx_db::unregister_class_cb(uint64_t id) {
	auto it = std::find_if(std::begin(impl::cb), std::end(impl::cb), [id](impl::cb_entry const& e) {return e.first==id;});
	if(it!=std::end(impl::cb))
		impl::cb.erase(it);
}

std::string const& tx_db::get_name() const {
	return pimpl->file_name;
}

tx_relation_handle tx_db::create_relation(const char *relation_name) const {
    auto& tmp_handle = pimpl->relation_by_name_map[relation_name];
    if(tmp_handle == 0) {
        tmp_handle = ++pimpl->relation_handle_counter;
        pimpl->relation_by_handle_map[tmp_handle] = relation_name;
    }
    return tmp_handle;
}

std::string const& tx_db::get_relation_name(tx_relation_handle relation_handle) const {
    return pimpl->relation_by_handle_map[relation_handle];
}
///////////////////////////////////////////////////////////////////////////////
/// tx_fiber
///////////////////////////////////////////////////////////////////////////////
namespace {
uint64_t fid_counter{0};
uint64_t tid_counter{0};
}
struct tx_fiber::impl {
	static std::vector<std::pair<uint64_t, tx_fiber_class_cb>> cb;
	using cb_entry = std::vector<std::pair<uint64_t, tx_fiber_class_cb>>::value_type;
};
std::vector<std::pair<uint64_t, tx_fiber::tx_fiber_class_cb>> tx_fiber::impl::cb;

tx_fiber::tx_fiber(const char *fiber_name, const char *fiber_kind, tx_db *tx_db_p)
: pimpl(new tx_fiber::impl)
, fiber_name(fiber_name)
, fiber_kind(fiber_kind)
, db(tx_db_p)
, id(++fid_counter)
{
	for(auto& e:impl::cb) e.second(*this, CREATE);
}

tx_fiber::~tx_fiber() {
	for(auto& e:impl::cb) e.second(*this, DELETE);
}

uint64_t tx_fiber::register_class_cb(tx_fiber_class_cb cb) {
	auto index = impl::cb.size()?impl::cb.back().first+1:0;
	impl::cb.push_back(std::make_pair(index, cb));
	return index;
}

void tx_fiber::unregister_class_cb(uint64_t id) {
	auto it = std::find_if(std::begin(impl::cb), std::end(impl::cb), [id](impl::cb_entry const& e) {return e.first==id;});
	if(it!=std::end(impl::cb))
		impl::cb.erase(it);
}
///////////////////////////////////////////////////////////////////////////////
/// tx_generator_base
///////////////////////////////////////////////////////////////////////////////
struct tx_generator_base::impl {
	static std::vector<std::pair<uint64_t, tx_generator_class_cb>> cb;
	using cb_entry = std::vector<std::pair<uint64_t, tx_generator_class_cb>>::value_type;
};
std::vector<std::pair<uint64_t, tx_generator_base::tx_generator_class_cb>> tx_generator_base::impl::cb;

tx_generator_base::tx_generator_base(std::string const& name, tx_fiber &fiber, std::string const& begin_attribute_name,	std::string const& end_attribute_name)
: fiber(fiber)
, generator_name(name)
, begin_attr_name(begin_attribute_name)
, end_attr_name(end_attribute_name)
, id(++fid_counter)
{
	for(auto& e:impl::cb) e.second(*this, CREATE);
}

tx_generator_base::~tx_generator_base() {
	for(auto& e:impl::cb) e.second(*this, DELETE);
}

uint64_t tx_generator_base::register_class_cb(tx_generator_class_cb cb) {
	auto index = impl::cb.size()?impl::cb.back().first+1:0;
	impl::cb.push_back(std::make_pair(index, cb));
	return index;
}

void tx_generator_base::unregister_class_cb(uint64_t id) {
	auto it = std::find_if(std::begin(impl::cb), std::end(impl::cb), [id](impl::cb_entry const& e) {return e.first==id;});
	if(it!=std::end(impl::cb))
		impl::cb.erase(it);
}

tx_handle tx_generator_base::begin_tx(value const& v, sc_core::sc_time const& begin_time, tx_relation_handle relation_handle, const tx_handle *other_handle_p) const {
	tx_handle hndl(*this, v, begin_time);
	if(other_handle_p)
		hndl.add_relation(relation_handle, *other_handle_p);
	return hndl;
}

void tx_generator_base::end_tx(tx_handle& t, value const& v, sc_core::sc_time const& end_time) const {
	t.deactivate(v, end_time);
}
///////////////////////////////////////////////////////////////////////////////
/// tx_handle
///////////////////////////////////////////////////////////////////////////////
struct tx_handle::impl {
	tx_generator_base const& gen;
	uint64_t id{std::numeric_limits<uint64_t>::max()};
	bool active{false};
	sc_core::sc_time begin_time, end_time;

	impl(tx_generator_base const& gen, sc_core::sc_time const& t):gen(gen), id(++tid_counter), active{true}, begin_time(t) {
		if(t<sc_core::sc_time_stamp()) {
            std::stringstream ss;
            ss << "transaction start time ("<<t<<") needs to be larger than current time (" << sc_core::sc_time_stamp()<<")";
            SC_REPORT_ERROR("tx_handle::tx_handle", ss.str().c_str());
		}
	}

	static std::vector<std::pair<uint64_t, tx_handle_class_cb>> cb;
	using cb_entry = std::vector<std::pair<uint64_t, tx_handle_class_cb>>::value_type;
	static std::vector<std::pair<uint64_t, tx_handle_attribute_cb>> acb;
	using acb_entry = std::vector<std::pair<uint64_t, tx_handle_attribute_cb>>::value_type;
	static std::vector<std::pair<uint64_t, tx_handle_relation_cb>> rcb;
	using rcb_entry = std::vector<std::pair<uint64_t, tx_handle_relation_cb>>::value_type;
};
std::vector<std::pair<uint64_t, tx_handle::tx_handle_class_cb>> tx_handle::impl::cb;
std::vector<std::pair<uint64_t, tx_handle::tx_handle_attribute_cb>> tx_handle::impl::acb;
std::vector<std::pair<uint64_t, tx_handle::tx_handle_relation_cb>> tx_handle::impl::rcb;

tx_handle::tx_handle(tx_generator_base const& gen, value const& v, sc_core::sc_time const& t)
: pimpl(std::make_shared<impl>(gen, t))
{
	for(auto& e:impl::cb) e.second(*this, BEGIN, v);
}

void tx_handle::deactivate(value const& v, sc_core::sc_time const& t) {
	if(t<sc_core::sc_time_stamp()) {
        std::stringstream ss;
        ss << "transaction end time ("<<t<<") needs to be larger than current time (" << sc_core::sc_time_stamp()<<")";
        SC_REPORT_ERROR("tx_handle::deactivate", ss.str().c_str());
	}
	pimpl->end_time=t;
	for(auto& e:impl::cb) e.second(*this, END, v);
	pimpl->active=false;
}

uint64_t tx_handle::register_class_cb(tx_handle_class_cb cb) {
	auto index = impl::cb.size()?impl::cb.back().first+1:0;
	impl::cb.push_back(std::make_pair(index, cb));
	return index;
}

void tx_handle::unregister_class_cb(uint64_t id) {
	auto it = std::find_if(std::begin(impl::cb), std::end(impl::cb), [id](impl::cb_entry const& e) {return e.first==id;});
	if(it!=std::end(impl::cb))
		impl::cb.erase(it);
}

uint64_t tx_handle::register_record_attribute_cb(tx_handle_attribute_cb cb) {
	auto index = impl::acb.size()?impl::acb.back().first+1:0;
	impl::acb.push_back(std::make_pair(index, cb));
	return index;
}

void tx_handle::unregister_record_attribute_cb(uint64_t id) {
	auto it = std::find_if(std::begin(impl::acb), std::end(impl::acb), [id](impl::acb_entry const& e) {return e.first==id;});
	if(it!=std::end(impl::acb))
		impl::acb.erase(it);
}

uint64_t tx_handle::register_relation_cb(tx_handle_relation_cb cb) {
	auto index = impl::rcb.size()?impl::rcb.back().first+1:0;
	impl::rcb.push_back(std::make_pair(index, cb));
	return index;
}

void tx_handle::unregister_relation_cb(uint64_t id) {
	auto it = std::find_if(std::begin(impl::rcb), std::end(impl::rcb), [id](impl::rcb_entry const& e) {return e.first==id;});
	if(it!=std::end(impl::rcb))
		impl::rcb.erase(it);
}

bool tx_handle::is_active() const {return pimpl->active; }

uint64_t tx_handle::get_id() const { return pimpl->id; }

void tx_handle::end_tx(const value &v, sc_core::sc_time const& end_sc_time) { pimpl->gen.end_tx(*this, v, end_sc_time); }

sc_core::sc_time tx_handle::get_begin_sc_time() const { return pimpl->begin_time; }

sc_core::sc_time tx_handle::get_end_sc_time() const { return pimpl->end_time; }

tx_fiber const& tx_handle::get_tx_fiber() const {return pimpl->gen.get_tx_fiber();}

tx_generator_base const& tx_handle::get_tx_generator_base() const { return pimpl->gen; }

void tx_handle::record_attribute(const char *name, value const& v) {
	for(auto& e:impl::acb) e.second(*this, name, v);
}

bool tx_handle::add_relation(tx_relation_handle relation_handle, tx_handle const& other_transaction_handle) {
	for(auto& e:impl::rcb) e.second(*this, other_transaction_handle, relation_handle);
	return true;
}

}

