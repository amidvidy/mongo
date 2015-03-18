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

#include <algorithm>

#include "mongo/bson/bsonobj.h"
#include "mongo/base/data_cursor.h"
#include "mongo/base/status_with.h"

namespace mongo {

    // Some of these could be in an anonymous namespace, but they are pulled out here
    // to be unit-testable
    StatusWith<BSONObj> readBSONObj(ConstDataCursor& reader,
                                    const ConstDataCursor rangeEnd);


    StatusWith<StringData> readString(ConstDataCursor& reader,
                                      const ConstDataCursor rangeEnd,
                                      std::size_t minLength,
                                      std::size_t maxLength);

    // T must satisfy EqualityComparable concept
    // passing T by value because this will only be used with primitives.
    // TODO: the existence of this helper indicates a need for a DataRange,
    // or BoundedDataCursor primitive.
    // returns cursor
    template<typename T>
    StatusWith<std::size_t> skipUntil(ConstDataCursor& reader,
                                      const ConstDataCursor rangeEnd,
                                      T sentinel,
                                      std::size_t minValidToSkip,
                                      std::size_t maxValidToSkip) {

        std::size_t maxElemsInRange = (rangeEnd.view() - reader.view()) / sizeof(T);

        // We can stop looking at the end of the range, or the last valid amount
        // of Ts to skip, whichever comes first.
        std::size_t maxToSkipOrEnd = std::min(maxElemsInRange, maxValidToSkip);

        for (std::size_t skipped = 0; skipped < maxToSkipOrEnd; ++skipped) {
            if (reader.readNative<T>() == sentinel) {
                if (skipped < minValidToSkip) {
                    return StatusWith<std::size_t>(ErrorCodes::FailedToParse);
                }
                return StatusWith<std::size_t>(skipped);
            }
            reader.skip<T>();
        }
        return StatusWith<std::size_t>(ErrorCodes::FailedToParse);
    }

}  // namespace mongo
