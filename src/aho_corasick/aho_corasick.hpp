/*
* Copyright (C) 2015 Christopher Gilbert.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#ifndef AHO_CORASICK_HPP
#define AHO_CORASICK_HPP

#include <algorithm>
#include <cassert>
#include <cctype>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <queue>
#include <utility>
#include <vector>

namespace aho_corasick {
	
	template <typename CharType, typename UniquePtr>
	class transition_map
	{
	public:
		typedef typename UniquePtr::pointer		ptr;
		typedef std::map<CharType, UniquePtr>	map_type;
		typedef typename map_type::size_type	size_type;
		typedef std::vector<ptr>				state_collection;
		typedef std::vector<CharType>			transition_collection;
		
	protected:
		map_type d_map;
		
	public:
		void set_transition(CharType character, ptr next)
		{
			d_map[character].reset(next);
		}
		
		size_type size() const { return d_map.size(); }
		void freeze() {}
		
		bool find(CharType character, ptr &result) const {
			auto it = d_map.find(character);
			if (it != d_map.end()) {
				result = it->second.get();
				return true;
			}
			
			return false;
		}
		
		
		state_collection get_states() const {
			state_collection result;
			for (auto it = d_map.cbegin(); it != d_map.cend(); ++it) {
				result.push_back(it->second.get());
			}
		}
		
		
		transition_collection get_transitions() const {
			transition_collection result;
			for (auto it = d_map.cbegin(); it != d_map.cend(); ++it) {
				result.push_back(it->first);
			}
		}
	};
	

	// class interval
	class interval {
		size_t d_start;
		size_t d_end;

	public:
		enum { max_pos = std::numeric_limits <size_t>::max() };

		interval(size_t start, size_t end)
			: d_start(start)
			, d_end(end) {}

		size_t get_start() const { return d_start; }
		size_t get_end() const { return d_end; }
		size_t size() const { return d_end - d_start + 1; }

		bool overlaps_with(const interval& other) const {
			return d_start <= other.d_end && d_end >= other.d_start;
		}

		bool overlaps_with(size_t point) const {
			return d_start <= point && point <= d_end;
		}

		bool operator <(const interval& other) const {
			return get_start() < other.get_start();
		}

		bool operator !=(const interval& other) const {
			return get_start() != other.get_start() || get_end() != other.get_end();
		}

		bool operator ==(const interval& other) const {
			return get_start() == other.get_start() && get_end() == other.get_end();
		}
	};

	// class interval_tree
	template<typename T>
	class interval_tree {
	public:
		using interval_collection = std::vector<T>;

	private:
		// class node
		class node {
			enum direction {
				LEFT, RIGHT
			};
			using node_ptr = std::unique_ptr<node>;

			size_t              d_point;
			node_ptr            d_left;
			node_ptr            d_right;
			interval_collection d_intervals;

		public:
			node(const interval_collection& intervals)
				: d_point(0)
				, d_left(nullptr)
				, d_right(nullptr)
				, d_intervals()
			{
				d_point = determine_median(intervals);
				interval_collection to_left, to_right;
				for (const auto& i : intervals) {
					if (i.get_end() < d_point) {
						to_left.push_back(i);
					} else if (i.get_start() > d_point) {
						to_right.push_back(i);
					} else {
						d_intervals.push_back(i);
					}
				}
				if (to_left.size() > 0) {
					d_left.reset(new node(to_left));
				}
				if (to_right.size() > 0) {
					d_right.reset(new node(to_right));
				}
			}

			size_t determine_median(const interval_collection& intervals) const {
				size_t start = interval::max_pos;
				size_t end = interval::max_pos;
				for (const auto& i : intervals) {
					size_t cur_start = i.get_start();
					size_t cur_end = i.get_end();
					if (start == interval::max_pos || cur_start < start) {
						start = cur_start;
					}
					if (end == interval::max_pos || cur_end > end) {
						end = cur_end;
					}
				}
				return (start + end) / 2;
			}

			interval_collection find_overlaps(const T& i) {
				interval_collection overlaps;
				if (d_point < i.get_start()) {
					add_to_overlaps(i, overlaps, find_overlapping_ranges(d_right, i));
					add_to_overlaps(i, overlaps, check_right_overlaps(i));
				} else if (d_point > i.get_end()) {
					add_to_overlaps(i, overlaps, find_overlapping_ranges(d_left, i));
					add_to_overlaps(i, overlaps, check_left_overlaps(i));
				} else {
					add_to_overlaps(i, overlaps, d_intervals);
					add_to_overlaps(i, overlaps, find_overlapping_ranges(d_left, i));
					add_to_overlaps(i, overlaps, find_overlapping_ranges(d_right, i));
				}
				return interval_collection(overlaps);
			}

		protected:
			void add_to_overlaps(const T& i, interval_collection& overlaps, interval_collection new_overlaps) const {
				for (const auto& cur : new_overlaps) {
					if (cur != i) {
						overlaps.push_back(cur);
					}
				}
			}

			interval_collection check_left_overlaps(const T& i) const {
				return interval_collection(check_overlaps(i, LEFT));
			}

			interval_collection check_right_overlaps(const T& i) const {
				return interval_collection(check_overlaps(i, RIGHT));
			}

			interval_collection check_overlaps(const T& i, direction d) const {
				interval_collection overlaps;
				for (const auto& cur : d_intervals) {
					switch (d) {
					case LEFT:
						if (cur.get_start() <= i.get_end()) {
							overlaps.push_back(cur);
						}
						break;
					case RIGHT:
						if (cur.get_end() >= i.get_start()) {
							overlaps.push_back(cur);
						}
						break;
					}
				}
				return interval_collection(overlaps);
			}

			interval_collection find_overlapping_ranges(node_ptr& node, const T& i) const {
				if (node) {
					return interval_collection(node->find_overlaps(i));
				}
				return interval_collection();
			}
		};
		node d_root;

	public:
		interval_tree(const interval_collection& intervals)
			: d_root(intervals) {}

		interval_collection remove_overlaps(const interval_collection& intervals) {
			interval_collection result(intervals.begin(), intervals.end());
			std::sort(result.begin(), result.end(), [](const T& a, const T& b) -> bool {
				if (b.size() - a.size() == 0) {
					return a.get_start() > b.get_start();
				}
				return a.size() > b.size();
			});
			std::set<T> remove_tmp;
			for (const auto& i : result) {
				if (remove_tmp.find(i) != remove_tmp.end()) {
					continue;
				}
				auto overlaps = find_overlaps(i);
				for (const auto& overlap : overlaps) {
					remove_tmp.insert(overlap);
				}
			}
			for (const auto& i : remove_tmp) {
				result.erase(
					std::find(result.begin(), result.end(), i)
				);
			}
			std::sort(result.begin(), result.end(), [](const T& a, const T& b) -> bool {
				return a.get_start() < b.get_start();
			});
			return interval_collection(result);
		}

		interval_collection find_overlaps(const T& i) {
			return interval_collection(d_root.find_overlaps(i));
		}
	};

	// class emit
	template<typename CharType>
	class emit: public interval {
	public:
		typedef std::basic_string<CharType>  string_type;
		typedef std::basic_string<CharType>& string_ref_type;

	private:
		string_type d_keyword;
		unsigned    d_index = 0;

	public:
		emit()
			: interval(interval::max_pos, interval::max_pos)
			, d_keyword() {}

		emit(size_t start, size_t end, string_type keyword, unsigned index)
			: interval(start, end)
			, d_keyword(keyword), d_index(index) {}

		string_type get_keyword() const { return string_type(d_keyword); }
		unsigned get_index() const { return d_index; }
		bool is_empty() const { return (get_start() == interval::max_pos && get_end() == interval::max_pos); }
	};

	// class token
	template<typename CharType>
	class token {
	public:
		enum token_type{
			TYPE_FRAGMENT,
			TYPE_MATCH,
		};

		using string_type     = std::basic_string<CharType>;
		using string_ref_type = std::basic_string<CharType>&;
		using emit_type       = emit<CharType>;

	private:
		token_type  d_type;
		string_type d_fragment;
		emit_type   d_emit;

	public:
		token(string_ref_type fragment)
			: d_type(TYPE_FRAGMENT)
			, d_fragment(fragment)
			, d_emit() {}

		token(string_ref_type fragment, const emit_type& e)
			: d_type(TYPE_MATCH)
			, d_fragment(fragment)
			, d_emit(e) {}

		bool is_match() const { return (d_type == TYPE_MATCH); }
		string_type get_fragment() const { return string_type(d_fragment); }
		emit_type get_emit() const { return d_emit; }
	};

	// class state
	template<typename CharType, template<typename, typename> class TransitionMap>
	class state {
	public:
		typedef state*                              ptr;
		typedef std::unique_ptr<state>              unique_ptr;
		typedef std::basic_string<CharType>         string_type;
		typedef std::basic_string<CharType>&        string_ref_type;
		typedef std::pair<string_type, unsigned>    key_index;
		typedef std::vector<key_index>              string_collection;
		typedef TransitionMap<CharType, unique_ptr> transition_map;

	private:
		size_t                         d_depth;
		uint32_t                       d_idx;
		uint32_t                       d_string_idx;
		ptr                            d_root;
		ptr                            d_parent;
		transition_map                 d_success;
		ptr                            d_failure;
		string_collection              d_emits;

	public:
		state(): state(0) {}

		state(size_t depth)
			: d_depth(depth)
			, d_idx(0)
			, d_string_idx(0)
			, d_root(depth == 0 ? this : nullptr)
			, d_parent(nullptr)
			, d_success()
			, d_failure(nullptr)
			, d_emits() {}

		ptr next_state(CharType character) const {
			return next_state(character, false);
		}

		ptr next_state_ignore_root_state(CharType character) const {
			return next_state(character, true);
		}

		ptr add_state(CharType character) {
			auto next = next_state_ignore_root_state(character);
			if (next == nullptr) {
				next = new state(d_depth + 1);
				next->set_parent(this);
				d_success.set_transition(character, next);
			}
			return next;
		}

		size_t get_depth() const { return d_depth; }

		void add_emit(string_ref_type keyword, unsigned index) {
			d_emits.emplace_back(keyword, index);
		}

		void add_emit(const string_collection& emits) {
			for (const auto& e : emits) {
				string_type str(e.first);
				add_emit(str, e.second);
			}
		}

		string_collection get_emits() const { return d_emits; }

		void clear_emits() { d_emits.clear(); }

		ptr failure() const { return d_failure; }

		void set_failure(ptr fail_state) { d_failure = fail_state; }

		ptr parent() const { return d_parent; }

		void set_parent(ptr parent_state) { d_parent = parent_state; }

		size_t index() const { return d_idx; }

		void set_index(uint32_t idx) { d_idx = idx; }

		size_t string_index() const { return d_string_idx; }

		void set_string_index(uint32_t idx) { d_string_idx = idx; }

		std::size_t goto_transition_count() const { return d_success.size(); }

		bool less_than_bfs_order(state const &other) const { return d_idx < other.d_idx; }
		bool greater_than_bfs_order(state const &other) const { return !less_than_bfs_order(other); }
		
		void freeze() { d_success.freeze(); }

		decltype(std::declval<transition_map>().get_states())
		get_states() const {
			return d_success.get_states();
		}

		decltype(std::declval<transition_map>().get_transitions())
		get_transitions() const {
			return d_success.get_transitions();
		}

	private:
		ptr next_state(CharType character, bool ignore_root_state) const {
			ptr result = nullptr;
			auto const found = d_success.find(character, result);
			if (!found && !ignore_root_state && d_root != nullptr)
				return d_root;
			
			return result;
		}
	};

	template<typename CharType, template<typename, typename> class TransitionMap = transition_map>
	class basic_trie {
	public:
		using string_type = std::basic_string < CharType > ;
		using string_ref_type = std::basic_string<CharType>&;

		typedef state<CharType, TransitionMap> state_type;
		typedef state_type*                    state_ptr_type;
		typedef token<CharType>                token_type;
		typedef emit<CharType>                 emit_type;
		typedef std::vector<token_type>        token_collection;
		typedef std::vector<emit_type>         emit_collection;

		class config {
			bool d_allow_overlaps;
			bool d_only_whole_words;
			bool d_case_insensitive;
			bool d_allow_substrings;
			bool d_store_states_in_bfs_order;

		public:
			config()
				: d_allow_overlaps(true)
				, d_only_whole_words(false)
				, d_case_insensitive(false)
				, d_allow_substrings(true)
				, d_store_states_in_bfs_order(false) {}

			bool is_allow_overlaps() const { return d_allow_overlaps; }
			void set_allow_overlaps(bool val) { d_allow_overlaps = val; }

			bool is_only_whole_words() const { return d_only_whole_words; }
			void set_only_whole_words(bool val) { d_only_whole_words = val; }

			bool is_case_insensitive() const { return d_case_insensitive; }
			void set_case_insensitive(bool val) { d_case_insensitive = val; }

			bool is_allow_substrings() const { return d_allow_substrings; }
			void set_allow_substrings(bool val) { d_allow_substrings = val; }
			
			bool is_store_states_in_bfs_order() const { return d_store_states_in_bfs_order; }
			void set_store_states_in_bfs_order(bool val) { d_store_states_in_bfs_order = val; }
		};

	private:
		std::unique_ptr<state_type> d_root;
		config                      d_config;
		bool                        d_postprocessed;
		unsigned                    d_num_keywords = 0;
		size_t                      d_state_count = 0;
		std::vector<state_ptr_type> d_states_in_bfs_order{};
		std::vector<state_ptr_type> d_final_states_in_bfs_order{};

	public:
		basic_trie(): basic_trie(config()) {}

		basic_trie(const config& c)
			: d_root(new state_type())
			, d_config(c)
			, d_postprocessed(false) {}

		basic_trie& case_insensitive() {
			d_config.set_case_insensitive(true);
			return (*this);
		}

		basic_trie& remove_overlaps() {
			d_config.set_allow_overlaps(false);
			return (*this);
		}

		basic_trie& only_whole_words() {
			d_config.set_only_whole_words(true);
			return (*this);
		}

		basic_trie& remove_substrings() {
			d_config.set_allow_substrings(false);
			return (*this);
		}
		
		basic_trie& store_states_in_bfs_order() {
			d_config.set_store_states_in_bfs_order(true);
			return (*this);
		}

		state_ptr_type insert(string_type keyword) {
			if (keyword.empty())
				return d_root.get();
			state_ptr_type cur_state = d_root.get();
			for (const auto& ch : keyword) {
				cur_state = cur_state->add_state(ch);
			}
			
			if (0 == cur_state->get_emits().size() || d_config.is_allow_substrings())
			{
				cur_state->add_emit(keyword, d_num_keywords++);
				return cur_state;
			}

			return nullptr;
		}

		template<class InputIterator>
		void insert(InputIterator first, InputIterator last) {
			for (InputIterator it = first; first != last; ++it) {
				insert(*it);
			}
		}

		size_t num_keywords() const { return d_num_keywords; }
		size_t num_states() const { return d_state_count; }
		
		state_ptr_type get_root() const { return d_root.get(); }
		void reset_root() { d_root.reset(new state_type()); }
		
		std::vector<state_ptr_type> const &get_states_in_bfs_order() const { return d_states_in_bfs_order; }
		std::vector<state_ptr_type> const &get_final_states_in_bfs_order() const { return d_final_states_in_bfs_order; }

		token_collection tokenise(string_type text) {
			token_collection tokens;
			auto collected_emits = parse_text(text);
			size_t last_pos = interval::max_pos;
			for (const auto& e : collected_emits) {
				if (e.get_start() - last_pos > 1) {
					tokens.push_back(create_fragment(e, text, last_pos));
				}
				tokens.push_back(create_match(e, text));
				last_pos = e.get_end();
			}
			if (text.size() - last_pos > 1) {
				tokens.push_back(create_fragment(typename token_type::emit_type(), text, last_pos));
			}
			return token_collection(tokens);
		}

		emit_collection parse_text(string_type text) {
			check_postprocess();
			size_t pos = 0;
			state_ptr_type cur_state = d_root.get();
			emit_collection collected_emits;
			for (auto c : text) {
				if (d_config.is_case_insensitive()) {
					c = std::tolower(c);
				}
				cur_state = get_state(cur_state, c);
				store_emits(pos, cur_state, collected_emits);
				pos++;
			}
			if (d_config.is_only_whole_words()) {
				remove_partial_matches(text, collected_emits);
			}
			if (!d_config.is_allow_overlaps()) {
				interval_tree<emit_type> tree(typename interval_tree<emit_type>::interval_collection(collected_emits.begin(), collected_emits.end()));
				auto tmp = tree.remove_overlaps(collected_emits);
				collected_emits.swap(tmp);
			}
			return emit_collection(collected_emits);
		}

		void check_postprocess() {
			if (!d_postprocessed) {
				assign_indices();

				if (!d_config.is_allow_substrings())
					remove_prefixes();
				
				construct_failure_states();
				
				// construct_failure_states clears emits; store final states
				// only after doing that.
				if (d_config.is_store_states_in_bfs_order())
				{
					for (auto const cur_state : d_states_in_bfs_order)
					{
						if (cur_state->get_emits().size())
							d_final_states_in_bfs_order.push_back(cur_state);
					}
				}

				d_postprocessed = true;
			}
		}

	private:
		token_type create_fragment(const typename token_type::emit_type& e, string_ref_type text, size_t last_pos) const {
			auto start = last_pos + 1;
			auto end = (e.is_empty()) ? text.size() : e.get_start();
			auto len = end - start;
			typename token_type::string_type str(text.substr(start, len));
			return token_type(str);
		}

		token_type create_match(const typename token_type::emit_type& e, string_ref_type text) const {
			auto start = e.get_start();
			auto end = e.get_end() + 1;
			auto len = end - start;
			typename token_type::string_type str(text.substr(start, len));
			return token_type(str, e);
		}

		void remove_partial_matches(string_ref_type search_text, emit_collection& collected_emits) const {
			size_t size = search_text.size();
			emit_collection remove_emits;
			for (const auto& e : collected_emits) {
				if ((e.get_start() == 0 || !std::isalpha(search_text.at(e.get_start() - 1))) &&
					(e.get_end() + 1 == size || !std::isalpha(search_text.at(e.get_end() + 1)))
					) {
					continue;
				}
				remove_emits.push_back(e);
			}
			for (auto& e : remove_emits) {
				collected_emits.erase(
					std::find(collected_emits.begin(), collected_emits.end(), e)
					);
			}
		}

		state_ptr_type get_state(state_ptr_type cur_state, CharType c) const {
			state_ptr_type result = cur_state->next_state(c);
			while (result == nullptr) {
				cur_state = cur_state->failure();
				result = cur_state->next_state(c);
			}
			return result;
		}

		void assign_indices()
		{
			// Set state indices in BFS order.
			std::queue<state_ptr_type> q;
			q.push(d_root.get());

			size_t i(0);
			while (!q.empty())
			{
				auto cur_state(q.front());
				cur_state->freeze();
				cur_state->set_index(i);
				
				if (d_config.is_store_states_in_bfs_order())
					d_states_in_bfs_order.push_back(cur_state);
				
				for (auto state_ptr : cur_state->get_states())
					q.push(state_ptr);

				q.pop();
				++i;
			}

			d_state_count = i;
		}
		
		void remove_prefixes_from_state(state_ptr_type cur_state)
		{
			auto const emit_count(cur_state->get_emits().size());
		
			assert(emit_count < 2);
		
			if (cur_state->goto_transition_count() && emit_count)
				cur_state->clear_emits();
		}

		void remove_prefixes() {
			// If a final state has a goto transition, it represents a prefix of another string.
			if (d_config.is_store_states_in_bfs_order())
			{
				assert(!d_states_in_bfs_order.empty());
				for (auto const cur_state : d_states_in_bfs_order)
					remove_prefixes_from_state(cur_state);
			}
			else
			{
				std::queue<state_ptr_type> q;
				q.push(d_root.get());

				while (!q.empty())
				{
					auto cur_state(q.front());
					remove_prefixes_from_state(cur_state);
					for (auto state_ptr : cur_state->get_states())
						q.push(state_ptr);
				
					q.pop();
				}
			}
		}

		void construct_failure_states() {
			std::queue<state_ptr_type> q;
			for (auto depth_one_state : d_root->get_states()) {
				depth_one_state->set_failure(d_root.get());
				q.push(depth_one_state);
			}

			while (!q.empty()) {
				auto cur_state = q.front();
				for (const auto& transition : cur_state->get_transitions()) {
					state_ptr_type target_state = cur_state->next_state(transition);
					q.push(target_state);

					state_ptr_type trace_failure_state = cur_state->failure();
					while (true)
					{
						while (trace_failure_state->next_state(transition) == nullptr) {
							trace_failure_state = trace_failure_state->failure();
						}
						state_ptr_type new_failure_state = trace_failure_state->next_state(transition);

						// If substrings are not allowed in the automaton and a final state was reached
						// with the transition, make the state non-final and find the next possible
						// failure state.
						if ((!d_config.is_allow_substrings()) && new_failure_state->get_emits().size())
						{
							new_failure_state->clear_emits();
							trace_failure_state = trace_failure_state->failure();
							continue;
						}

						target_state->set_failure(new_failure_state);
						target_state->add_emit(new_failure_state->get_emits());
						break;
					}
				}
				q.pop();
			}
		}

		void store_emits(size_t pos, state_ptr_type cur_state, emit_collection& collected_emits) const {
			auto emits = cur_state->get_emits();
			if (!emits.empty()) {
				for (const auto& str : emits) {
					auto emit_str = typename emit_type::string_type(str.first);
					collected_emits.push_back(emit_type(pos - emit_str.size() + 1, pos, emit_str, str.second));
				}
			}
		}
	};

	typedef basic_trie<char>     trie;
	typedef basic_trie<wchar_t>  wtrie;


} // namespace aho_corasick

#endif // AHO_CORASICK_HPP
