/**
*    Copyright (C) 2015 MongoDB Inc.
*
*    This program is free software: you can redistribute it and/or  modify
*    it under the terms of the GNU Affero General Public License, version 3,
*    as published by the Free Software Foundation.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Affero General Public License for more details.
*
*    You should have received a copy of the GNU Affero General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*    As a special exception, the copyright holders give permission to link the
*    code of portions of this program with the OpenSSL library under certain
*    conditions as described in each individual source file and distribute
*    linked combinations including the program with the OpenSSL library. You
*    must comply with the GNU Affero General Public License in all respects for
*    all of the code used other than as permitted herein. If you modify file(s)
*    with this exception, you may extend this exception to your version of the
*    file(s), but you are not obligated to do so. If you do not wish to do so,
*    delete this exception statement from your version. If you delete this
*    exception statement from all source files in the program, then also delete
*    it in the license file.
*/

#include "mongo/platform/basic.h"

#include "mongo/util/net/command/document_range.h"

#include <utility>
#include <tuple>

#include "mongo/util/assert_util.h"
#include "mongo/util/net/command/command_parse_util.h"

namespace mongo {

    DocumentRange::DocumentRange(ConstDataCursor begin, ConstDataCursor end)
        : _begin{std::move(begin.view())}
        , _end{std::move(end.view())}
    {}

    DocumentRange::const_iterator DocumentRange::begin() const {
        return const_iterator{ConstDataCursor{_begin}, ConstDataCursor{_end}};
    }

    DocumentRange::const_iterator DocumentRange::end() const {
        return const_iterator{ConstDataCursor{_end}, ConstDataCursor{_end}};
    }

    DocumentRange::const_iterator::const_iterator(ConstDataCursor pos,
                                                  ConstDataCursor rangeEnd)
        : _nextDoc{std::move(pos.view())}
        , _rangeEnd{std::move(rangeEnd.view())} {

        operator++();
    }

    DocumentRange::const_iterator::reference
    DocumentRange::const_iterator::operator*() const {
        return _obj;
    }

    DocumentRange::const_iterator::pointer
    DocumentRange::const_iterator::operator->() const {
        return &_obj;
    }

    DocumentRange::const_iterator&
    DocumentRange::const_iterator::operator++() {
        auto pos = ConstDataCursor{_nextDoc};
        auto parsedObj = readBSONObj(pos, ConstDataCursor{_rangeEnd});
        uassertStatusOK(parsedObj.getStatus());
        _obj = std::move(parsedObj.getValue());
        _nextDoc = std::move(pos.view());
        return *this;
    }

    DocumentRange::const_iterator
    DocumentRange::const_iterator::operator++(int) {
        auto pre = DocumentRange::const_iterator(ConstDataCursor{_nextDoc}, 
                                                 ConstDataCursor{_rangeEnd});
        operator++();
        return pre;
    }

    bool operator==(const DocumentRange::const_iterator& lhs, 
                    const DocumentRange::const_iterator& rhs) {
        return std::tie(lhs._nextDoc, lhs._rangeEnd, lhs._obj) ==
               std::tie(rhs._nextDoc, rhs._rangeEnd, rhs._obj);
    }

    bool operator!=(const DocumentRange::const_iterator& lhs,
                    const DocumentRange::const_iterator& rhs) {
        return !(lhs == rhs);
    }

}  // namespace mongo
