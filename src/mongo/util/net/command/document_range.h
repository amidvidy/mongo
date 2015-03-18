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

#pragma once

#include <cstddef>
#include <iterator>

#include "mongo/base/data_cursor.h"
#include "mongo/bson/bsonobj.h"

namespace mongo {

    // A read-only view over a sequence of BSON documents.
    class DocumentRange {
    public:
        class const_iterator;

        DocumentRange(ConstDataCursor begin, ConstDataCursor end);

        const_iterator begin() const;
        const_iterator end() const;
    private:
        const char* _begin;
        const char* _end;
    };

    class DocumentRange::const_iterator
        : public std::iterator<std::forward_iterator_tag,
                               BSONObj,
                               std::ptrdiff_t,
                               const BSONObj*,
                               const BSONObj&> {
    public:
        explicit const_iterator(ConstDataCursor pos, ConstDataCursor rangeEnd); // ??
        
        reference operator*() const;
        pointer operator->() const;
        
        const_iterator& operator++();
        const_iterator operator++(int);

        friend bool operator==(const const_iterator&, const const_iterator&);
        friend bool operator!=(const const_iterator&, const const_iterator&);
    private:
        const char* _nextDoc;
        const char* _rangeEnd;
        BSONObj _obj;
    };

}  // namespace mongo
