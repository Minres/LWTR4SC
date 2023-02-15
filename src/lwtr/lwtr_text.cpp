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
#include <array>
#include <cerrno>
#include <cstring>
#include <fmt/format.h>
#include <fmt/os.h>
#include <fmt/printf.h>
#include <sstream>
#include <string>
#include <numeric>
#include <cstring>
#include <fstream>
#ifdef WITH_ZLIB
#include <zlib.h>
#endif
#ifdef WITH_LZ4
#include "util/lz4_streambuf.h"
#endif

namespace lwtr {
namespace {
// ----------------------------------------------------------------------------
class PlainWriter {
	std::ofstream out;
public:
	PlainWriter(const std::string& name) : out(name) {}
	~PlainWriter() {
		if(out.is_open()) out.close();
	}
	bool is_open(){return out.is_open();}

	inline void write(std::string const& buf) {
		out.write(buf.c_str(), buf.size());
	}

	inline void write(std::string && buf) {
		out.write(buf.c_str(), buf.size());
	}

	static std::string const extension;
};
std::string const PlainWriter::extension{"lwtrt"};
// ----------------------------------------------------------------------------
#ifdef WITH_ZLIB
class GZipWriter {
	gzFile file_p = nullptr;
public:
	GZipWriter(const std::string& name) {
		file_p = gzopen(name.c_str(), "wb1"); // f, h, R
	}
	~GZipWriter() {
		if(file_p)
			gzclose(file_p);
	}
	bool is_open(){return file_p;}

	inline void write(std::string const& buf) {
		 gzprintf(file_p, buf.c_str());
	}

	inline void write(std::string && buf) {
		 gzprintf(file_p, buf.c_str());
	}

	static std::string const extension;
};
std::string const GZipWriter::extension{"lwtrt.gz"};
#endif
// ----------------------------------------------------------------------------
#ifdef WITH_LZ4
class LZ4Writer {
	std::ofstream ofs;
	std::unique_ptr<util::lz4c_steambuf> strbuf;
public:
	std::ostream out;
	LZ4Writer(const std::string& name)
	: ofs(name, std::ios::binary|std::ios::trunc)
	, strbuf(new util::lz4c_steambuf(ofs, 8192))
	, out(strbuf.get())
	{}
	~LZ4Writer() {
		if(is_open()){
			strbuf->close();
			ofs.close();
		}
	}

	bool is_open(){return ofs.is_open();}

	inline void write(std::string const& buf) {
		out.write(buf.c_str(), buf.size());
	}

	inline void write(std::string && buf) {
		out.write(buf.c_str(), buf.size());
	}

	static std::string const extension;
};
std::string const LZ4Writer::extension{"lwtrt.lz"};
#endif
// ----------------------------------------------------------------------------
template<typename WRITER>
struct Writer {
	std::unique_ptr<WRITER> writer;
	Writer(const std::string& name): writer(new WRITER(name))
	{}

	Writer()
	{}

	inline bool open(const std::string& name) {
		writer.reset(new WRITER(name));
		return writer->is_open();
	}

	inline void close() { delete writer.release(); }

	inline bool is_open() { return writer->is_open();}

	template <typename S, typename... Args>
	inline void write(S const& format, Args&& ...args) {
		writer->write(fmt::format(format, args...));
	}

	inline std::string const& get_extension(){ return WRITER::extension;}

	inline static Writer &get() {
		static Writer db;
		return db;
	}
};
// ----------------------------------------------------------------------------
template<typename DB>
void tx_db_cbf(tx_db const& _tx_db, callback_reason reason) {
	static std::string file_name("tx_default");
	switch(reason) {
	case CREATE: {
		if(_tx_db.get_name().length() != 0) {
			file_name = _tx_db.get_name();
		}
		file_name+="."+Writer<DB>::get().get_extension();
		if(Writer<DB>::get().open(file_name)) {
			std::stringstream ss;
			ss << "opening file " << file_name;
			SC_REPORT_INFO(__FUNCTION__, ss.str().c_str());
		} else {
			std::stringstream ss;
			ss << "Can't open text recording file. " << strerror(errno);
			SC_REPORT_ERROR(__FUNCTION__, ss.str().c_str());
		}
	}
	break;
	case DELETE: {
		std::stringstream ss;
		ss << "closing file " << file_name;
		SC_REPORT_INFO(__FUNCTION__, ss.str().c_str());
		Writer<DB>::get().close();
	}
	break;
	default:
		SC_REPORT_ERROR(__FUNCTION__, "Unknown reason in tx_db callback");
	}
}
// ----------------------------------------------------------------------------
template<typename DB>
void tx_fiber_cbf(const tx_fiber& s, callback_reason reason) {
	if(reason == CREATE) {
		Writer<DB>::get().write("scv_tr_stream (ID {}, name \"{}\", kind \"{}\")\n", s.get_id(), s.get_name(),
				s.get_fiber_kind().length() ? s.get_fiber_kind() : "<no_stream_kind>");
	}
}
// ----------------------------------------------------------------------------
template<typename DB>
struct value_visitor {
	uint64_t const tx_id;

	mutable std::vector<std::string> hier_name;

	value_visitor(uint64_t tx_id, std::string const& name):tx_id(tx_id) {
		if(name.length()) hier_name.push_back(name);
	}

	std::string get_full_name() const {
		if(hier_name.empty()) return "unnamed";
		auto buf_size = std::accumulate(hier_name.begin(), hier_name.end(), 0U,
				[](size_t a, std::string const& b){return a+b.size();});
		std::string res;
		res.reserve(buf_size+hier_name.size());
		bool need_sep = false;
		for(auto&e: hier_name) {
			if(need_sep) res.append(".");
			res.append(e);
		}
		return res;
	}

	void operator()(const lwtr::no_data& v) const {	}
	void operator()(std::string const& v) const {
		Writer<DB>::get().write("tx_record_attribute {} \"{}\" STRING = \"{}\"\n", tx_id, get_full_name(), v);
	}
	void operator()(double v) const {
		Writer<DB>::get().write("tx_record_attribute {} \"{}\" FLOATING_POINT_NUMBER = {}\n", tx_id, get_full_name(), v);
	}
	void operator()(char const* v) const {
		Writer<DB>::get().write("tx_record_attribute {} \"{}\" STRING = \"{}\"\n", tx_id, get_full_name(), v);
	}
	void operator()(bool v) const {
		Writer<DB>::get().write("tx_record_attribute {} \"{}\" BOOLEAN = {}\n", tx_id, get_full_name(), v ? "true" : "false");
	}
	void operator()(uint8_t v) const {
		Writer<DB>::get().write("tx_record_attribute {} \"{}\" UNSIGNED = {}\n", tx_id, get_full_name(), v);
	}
	void operator()(uint16_t v) const {
		Writer<DB>::get().write("tx_record_attribute {} \"{}\" UNSIGNED = {}\n", tx_id, get_full_name(), v);
	}
	void operator()(uint32_t v) const {
		Writer<DB>::get().write("tx_record_attribute {} \"{}\" UNSIGNED = {}\n", tx_id, get_full_name(), v);
	}
	void operator()(uint64_t v) const {
		Writer<DB>::get().write("tx_record_attribute {} \"{}\" UNSIGNED = {}\n", tx_id, get_full_name(), v);
	}
	void operator()(int8_t v) const {
		Writer<DB>::get().write("tx_record_attribute {} \"{}\" INTEGER = {}\n", tx_id, get_full_name(), v);
	}
	void operator()(int16_t v) const {
		Writer<DB>::get().write("tx_record_attribute {} \"{}\" INTEGER = {}\n", tx_id, get_full_name(), v);
	}
	void operator()(int32_t v) const {
		Writer<DB>::get().write("tx_record_attribute {} \"{}\" INTEGER = {}\n", tx_id, get_full_name(), v);
	}
	void operator()(int64_t v) const {
		Writer<DB>::get().write("tx_record_attribute {} \"{}\" INTEGER = {}\n", tx_id, get_full_name(), v);
	}
	void operator()(sc_dt::sc_bv_base const& v) const {
		Writer<DB>::get().write("tx_record_attribute {} \"{}\" BIT_VECTOR = \"{}\"\n", tx_id, get_full_name(), v.to_string());
	}
	void operator()(sc_dt::sc_lv_base const& v) const {
		Writer<DB>::get().write("tx_record_attribute {} \"{}\" LOGIC_VECTOR = \"{}\"\n", tx_id, get_full_name(), v.to_string());
	}
	void operator()(sc_core::sc_time v) const {
		Writer<DB>::get().write("tx_record_attribute {} \"{}\" STRING = \"{}\"\n", tx_id, get_full_name(), v.to_string());
	}
	void operator()(lwtr::object const& v) const {
		for(auto& e:v) {
			hier_name.push_back(std::get<0>(e));
			mpark::visit(*this, std::get<1>(e) );
			hier_name.pop_back();
		}
	}
};
// ----------------------------------------------------------------------------
template<typename DB>
void tx_generator_cbf(const tx_generator_base& g, callback_reason reason) {
	if(reason != CREATE)
		return;
	if(!Writer<DB>::get().is_open())
		return;
	Writer<DB>::get().write("scv_tr_generator (ID {}, name \"{}\", scv_tr_stream {},\n)\n", g.get_id(), g.get_name(), g.get_tx_fiber().get_id());
}
// ----------------------------------------------------------------------------
template<typename DB>
void tx_handle_cbf(const tx_handle& t, callback_reason reason, value const& v) {
	if(!Writer<DB>::get().is_open())
		return;
	if(t.get_tx_fiber().get_tx_db() == nullptr)
		return;
	if(t.get_tx_fiber().get_tx_db()->get_recording() == false)
		return;
	switch(reason) {
	case BEGIN: {
		Writer<DB>::get().write("tx_begin {} {} {}\n", t.get_id(), t.get_tx_generator_base().get_id(),
				t.get_begin_sc_time().to_string());
		mpark::visit( value_visitor<DB>(t.get_id(),  t.get_tx_generator_base().get_begin_attribute_name()), v);
	} break;
	case END: {
		mpark::visit( value_visitor<DB>(t.get_id(),  t.get_tx_generator_base().get_end_attribute_name()), v);
		Writer<DB>::get().write("tx_end {} {} {}\n", t.get_id(), t.get_tx_generator_base().get_id(),
				t.get_end_sc_time().to_string());
	} break;
	default:;
	}
}
// ----------------------------------------------------------------------------
template<typename DB>
void tx_handle_record_attribute_cbf(tx_handle const& t, const char* attribute_name, value const& v) {
	if(t.get_tx_fiber().get_tx_db() == nullptr)
		return;
	if(t.get_tx_fiber().get_tx_db()->get_recording() == false)
		return;
	if(!Writer<DB>::get().is_open())
		return;
	std::string tmp_str = attribute_name == nullptr ? "" : attribute_name;
	mpark::visit( value_visitor<DB>(t.get_id(),  tmp_str), v);
}
// ----------------------------------------------------------------------------
template<typename DB>
void tx_handle_relation_cbf(const tx_handle& tr_1, const tx_handle& tr_2, tx_relation_handle relation_handle) {
	if(tr_1.get_tx_fiber().get_tx_db() == nullptr)
		return;
	if(tr_1.get_tx_fiber().get_tx_db()->get_recording() == false)
		return;
	if(!Writer<DB>::get().is_open())
		return;
	if(Writer<DB>::get().is_open()) {
		Writer<DB>::get().write("tx_relation \"{}\" {} {}\n",
				tr_1.get_tx_fiber().get_tx_db()->get_relation_name(relation_handle), tr_1.get_id(),
				tr_2.get_id());
	}
}
// ----------------------------------------------------------------------------
}
// ----------------------------------------------------------------------------
void tx_text_init() {
	tx_db::register_class_cb(tx_db_cbf<PlainWriter>);
	tx_fiber::register_class_cb(tx_fiber_cbf<PlainWriter>);
	tx_generator_base::register_class_cb(tx_generator_cbf<PlainWriter>);
	tx_handle::register_class_cb(tx_handle_cbf<PlainWriter>);
	tx_handle::register_record_attribute_cb(tx_handle_record_attribute_cbf<PlainWriter>);
	tx_handle::register_relation_cb(tx_handle_relation_cbf<PlainWriter>);
}
#ifdef WITH_ZLIB
void tx_text_gz_init() {
    tx_db::register_class_cb(tx_db_cbf<GZipWriter>);
    tx_fiber::register_class_cb(tx_fiber_cbf<GZipWriter>);
    tx_generator_base::register_class_cb(tx_generator_cbf<GZipWriter>);
    tx_handle::register_class_cb(tx_handle_cbf<GZipWriter>);
    tx_handle::register_record_attribute_cb(tx_handle_record_attribute_cbf<GZipWriter>);
    tx_handle::register_relation_cb(tx_handle_relation_cbf<GZipWriter>);
}
#endif
#ifdef WITH_LZ4
void tx_text_lz4_init() {
    tx_db::register_class_cb(tx_db_cbf<LZ4Writer>);
    tx_fiber::register_class_cb(tx_fiber_cbf<LZ4Writer>);
    tx_generator_base::register_class_cb(tx_generator_cbf<LZ4Writer>);
    tx_handle::register_class_cb(tx_handle_cbf<LZ4Writer>);
    tx_handle::register_record_attribute_cb(tx_handle_record_attribute_cbf<LZ4Writer>);
    tx_handle::register_relation_cb(tx_handle_relation_cbf<LZ4Writer>);
}
#endif
} // namespace lwtr
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
