// Set operations on sorted trixel and range lists: union, intersection, exclusive disjunction and simplify.
/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 * Copyright (c) 2015-2019 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "htm.h"

#include <algorithm>                              // for std::merge
#include <iterator>                               // for std::back_inserter, std::make_move_iterator
#include <utility>                                // for std::move, std::swap


std::vector<std::string>
HTM::trixel_union(std::vector<std::string>&& txs1, std::vector<std::string>&& txs2)
{
	if (txs1.empty()) {
		return std::move(txs2);
	}

	if (txs2.empty()) {
		return std::move(txs1);
	}

	std::vector<std::string> res;
	res.reserve(txs1.size() + txs2.size());

	std::merge(std::make_move_iterator(txs1.begin()), std::make_move_iterator(txs1.end()),
		std::make_move_iterator(txs2.begin()), std::make_move_iterator(txs2.end()), std::back_inserter(res));

	return res;
}


std::vector<std::string>
HTM::trixel_intersection(std::vector<std::string>&& txs1, std::vector<std::string>&& txs2)
{
	if (txs1.empty() || txs2.empty()) {
		return std::vector<std::string>();
	}

	std::vector<std::string> res;
	res.reserve(txs1.size() < txs2.size() ? txs1.size() : txs2.size());

	const auto it1_e = txs1.end();
	const auto it2_e = txs2.end();
	auto it1 = txs1.begin();
	auto it2 = txs2.begin();

	while (it1 != it1_e && it2 != it2_e) {
		if (*it1 > *it2) {
			if (it1->find(*it2) == 0) {
				res.push_back(std::move(*it1));
				++it1;
			} else {
				++it2;
			}
		} else {
			if (it2->find(*it1) == 0) {
				res.push_back(std::move(*it2));
				++it2;
			} else {
				++it1;
			}
		}
	}

	return res;
}


/*
 * Fills result with the trixels that conform to the father except trixel's son.
 *   Father      Son			 Trixels back:
 *     /\	     /\
 *    /__\      /__\	   =>	     __
 *   /\  /\					       /\  /\
 *  /__\/__\					  /__\/__\
 *
 * result is sort.
 */
static inline void exclusive_disjunction(std::vector<std::string>& result, const std::string& father, const std::string& son, size_t depth) {
	if (depth != 0u) {
		switch (son.at(father.length())) {
			case '0':
				exclusive_disjunction(result, father + std::string(1, '0'), son, --depth);
				result.push_back(father + std::string(1, '1'));
				result.push_back(father + std::string(1, '2'));
				result.push_back(father + std::string(1, '3'));
				return;
			case '1':
				result.push_back(father + std::string(1, '0'));
				exclusive_disjunction(result, father + std::string(1, '1'), son, --depth);
				result.push_back(father + std::string(1, '2'));
				result.push_back(father + std::string(1, '3'));
				return;
			case '2':
				result.push_back(father + std::string(1, '0'));
				result.push_back(father + std::string(1, '1'));
				exclusive_disjunction(result, father + std::string(1, '2'), son, --depth);
				result.push_back(father + std::string(1, '3'));
				return;
			case '3':
				result.push_back(father + std::string(1, '0'));
				result.push_back(father + std::string(1, '1'));
				result.push_back(father + std::string(1, '2'));
				exclusive_disjunction(result, father + std::string(1, '3'), son, --depth);
				return;
		}
	}
}


std::vector<std::string>
HTM::trixel_exclusive_disjunction(std::vector<std::string>&& txs1, std::vector<std::string>&& txs2)
{
	if (txs1.empty()) {
		return std::move(txs2);
	}

	if (txs2.empty()) {
		return std::move(txs1);
	}

	auto it1 = txs1.begin();
	auto it2 = txs2.begin();

	while (it1 != txs1.end() && it2 != txs2.end()) {
		if (*it1 > *it2) {
			if (it1->find(*it2) == 0) {
				size_t depth = it1->length() - it2->length();
				std::vector<std::string> subtrixels;
				subtrixels.reserve(3 * depth);
				exclusive_disjunction(subtrixels, *it2, *it1, depth);
				it2 = txs2.erase(it2);
				it2 = txs2.insert(it2, std::make_move_iterator(subtrixels.begin()), std::make_move_iterator(subtrixels.end()));
				it1 = txs1.erase(it1);
			} else {
				++it2;
			}
		} else {
			if (it2->find(*it1) == 0) {
				size_t depth = it2->length() - it1->length();
				std::vector<std::string> subtrixels;
				subtrixels.reserve(3 * depth);
				exclusive_disjunction(subtrixels, *it1, *it2, depth);
				it1 = txs1.erase(it1);
				it1 = txs1.insert(it1, std::make_move_iterator(subtrixels.begin()), std::make_move_iterator(subtrixels.end()));
				it2 = txs2.erase(it2);
			} else {
				++it1;
			}
		}
	}

	std::vector<std::string> res;
	res.reserve(txs1.size() + txs2.size());

	std::merge(std::make_move_iterator(txs1.begin()), std::make_move_iterator(txs1.end()),
		std::make_move_iterator(txs2.begin()), std::make_move_iterator(txs2.end()), std::back_inserter(res));

	return res;
}


std::vector<range_t>
HTM::range_union(std::vector<range_t>&& rs1, std::vector<range_t>&& rs2)
{
	if (rs1.empty()) {
		return std::move(rs2);
	}

	if (rs2.empty()) {
		return std::move(rs1);
	}

	std::vector<range_t> res;
	res.reserve(rs1.size() + rs2.size());

	const auto it1_e = rs1.end();
	const auto it2_e = rs2.end();
	auto it1 = rs1.begin();
	auto it2 = rs2.begin();

	res.push_back(it1->start < it2->start ? std::move(*it1++) : std::move(*it2++));

	while (it1 != it1_e && it2 != it2_e) {
		if (it1->start < it2->start) {
			insertGreaterRange(res, std::move(*it1));
			++it1;
		} else {
			insertGreaterRange(res, std::move(*it2));
			++it2;
		}
	}

	while (it1 != it1_e) {
		auto& prev = res.back();
		if (prev.end < it1->start - 1) {   // (start - 1) for join adjacent integer ranges.
			res.insert(res.end(), std::make_move_iterator(it1), std::make_move_iterator(it1_e));
			break;
		}
		if (prev.end < it1->end) {  // if ranges overlap, previous range end is updated.
			prev.end = it1->end;
		}
		++it1;
	}

	while (it2 != it2_e) {
		auto& prev = res.back();
		if (prev.end < it2->start - 1) {  // (start - 1) for join adjacent integer ranges.
			res.insert(res.end(), std::make_move_iterator(it2), std::make_move_iterator(it2_e));
			break;
		}
		if (prev.end < it2->end) {  // if ranges overlap, previous range end is updated.
			prev.end = it2->end;
		}
		++it2;
	}

	return res;
}


std::vector<range_t>
HTM::range_intersection(std::vector<range_t>&& rs1, std::vector<range_t>&& rs2)
{
	if (rs1.empty() || rs2.empty() || rs1.front().start > rs2.back().end || rs1.back().end < rs2.front().start) {
		return std::vector<range_t>();
	}

	std::vector<range_t> res;
	res.reserve(rs1.size() < rs2.size() ? rs1.size() : rs2.size());

	const auto it1_e = rs1.end();
	const auto it2_e = rs2.end();
	auto it1 = rs1.begin();
	auto it2 = rs2.begin();

	while (it1 != it1_e && it2 != it2_e) {
		if (it1->start < it2->start) {
			if (it1->end >= it2->start) {
				if (it1->end <= it2->end) {
					insertGreaterRange(res, range_t(it2->start, it1->end));
					++it1;
				} else {
					insertGreaterRange(res, *it2);
					++it2;
				}
			} else {
				++it1;
			}
		} else {
			if (it2->end >= it1->start) {
				if (it2->end <= it1->end) {
					insertGreaterRange(res, range_t(it1->start, it2->end));
					++it2;
				} else {
					insertGreaterRange(res, *it1);
					++it1;
				}
			} else {
				++it2;
			}
		}
	}

	return res;
}


static inline void exclusive_disjunction(std::vector<range_t>& res, range_t& range) {
	if (res.empty()) {
		res.push_back(std::move(range));
		return;
	}

	auto& prev = res.back();
	if (prev.start == range.start) {
		if (prev.end > range.end) {
			prev.start = range.end + 1;
		} else if (prev.end < range.end) {
			prev.start = prev.end + 1;
			prev.end = range.end;
		} else {
			res.pop_back();
		}
	} else {
		if (range.start < prev.start) {
			std::swap(prev, range);
		}
		if (prev.end < range.start) {
			HTM::insertGreaterRange(res, std::move(range));
		} else {
			if (prev.end == range.end) {
				prev.end = --range.start;
			} else if (prev.end < range.end) {
				std::swap(++prev.end, --range.start);
				HTM::insertGreaterRange(res, std::move(range));
			} else {
				std::swap(prev.end, --range.start);
				std::swap(range.start, ++range.end);
				HTM::insertGreaterRange(res, std::move(range));
			}
		}
	}
}


std::vector<range_t>
HTM::range_exclusive_disjunction(std::vector<range_t>&& rs1, std::vector<range_t>&& rs2)
{
	if (rs1.empty()) {
		return std::move(rs2);
	}

	if (rs2.empty()) {
		return std::move(rs1);
	}

	std::vector<range_t> res;
	res.reserve(rs1.size() + rs2.size());

	const auto it1_e = rs1.end();
	const auto it2_e = rs2.end();
	auto it1 = rs1.begin();
	auto it2 = rs2.begin();

	res.push_back(it1->start < it2->start ? *it1++ : *it2++);

	while (it1 != it1_e && it2 != it2_e) {
		if (it1->start < it2->start) {
			exclusive_disjunction(res, *it1);
			++it1;
		} else {
			exclusive_disjunction(res, *it2);
			++it2;
		}
	}

	while (it1 != it1_e) {
		auto& prev = res.back();
		if (prev.end < it1->start) {
			res.insert(res.end(), std::make_move_iterator(it1), std::make_move_iterator(it1_e));
			break;
		}
		exclusive_disjunction(res, *it1);
		++it1;
	}

	while (it2 != it2_e) {
		auto& prev = res.back();
		if (prev.end < it2->start) {
			res.insert(res.end(), std::make_move_iterator(it2), std::make_move_iterator(it2_e));
			break;
		}
		exclusive_disjunction(res, *it2);
		++it2;
	}

	return res;
}


void
HTM::simplifyTrixels(std::vector<std::string>& trixels)
{
	if (trixels.empty()) {
		return;
	}

	// Delete duplicates and redundants.
	for (auto it = trixels.begin(); it != trixels.end() - 1; ) {
		auto n_it = it + 1;
		if (n_it->find(*it) == 0) {
			trixels.erase(n_it);
		} else {
			++it;
		}
	}

	// To compact.
	for (auto it = trixels.begin(); trixels.size() > 3 && it < trixels.end() - 3; ) {
		auto tlen = it->length();
		if (tlen > 2) {
			auto it_j = it + 1;
			auto it_k = it + 2;
			auto it_l = it + 3;
			if (it_j->length() == tlen && it_k->length() == tlen && it_l->length() == tlen) {
				--tlen;
				bool equals = true;
				for (size_t p = 0; p < tlen; ++p) {
					const auto& c = (*it)[p];
					if ((*it_j)[p] != c || (*it_k)[p] != c || (*it_l)[p] != c) {
						equals = false;
						break;
					}
				}
				if (equals) {
					it_j = trixels.erase(it_j);
					it_j = trixels.erase(it_j);
					trixels.erase(it_j);
					it->pop_back();
					it_j = trixels.begin();
					it < it_j + 3 ? it = it_j : it -= 3;
					continue;
				}
			}
		}
		++it;
	}
}


void
HTM::simplifyRanges(std::vector<range_t>& ranges)
{
	if (!ranges.empty()) {
		for (auto it = ranges.begin() + 1; it != ranges.end(); ) {
			// Get previous range.
			auto tmp = it - 1;
			// Test if current range is not overlapping with previous.
			if (tmp->end < it->start - 1) {   // (start - 1) for join adjacent integer ranges.
				++it;
			} else if (tmp->end < it->end) {  // if ranges overlap, previous range end is updated.
				tmp->end = it->end;
				it = ranges.erase(it);
			} else {
				// Ranges overlapping.
				it = ranges.erase(it);
			}
		}
	}
}
