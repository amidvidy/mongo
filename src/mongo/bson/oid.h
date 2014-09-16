// oid.h

/*    Copyright 2009 10gen Inc.
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
 *    must comply with the GNU Affero General Public License in all respects
 *    for all of the code used other than as permitted herein. If you modify
 *    file(s) with this exception, you may extend this exception to your
 *    version of the file(s), but you are not obligated to do so. If you do not
 *    wish to do so, delete this exception statement from your version. If you
 *    delete this exception statement from all source files in the program,
 *    then also delete it in the license file.
 */

#pragma once

#include <string>

#include "mongo/base/data_view.h"
#include "mongo/bson/util/misc.h"
#include "mongo/platform/endian.h"
#include "mongo/stdx/type_traits.h"
#include "mongo/util/hex.h"

namespace mongo {

    /** 
     * Object ID type.
     * BSON objects typically have an _id field for the object id.  This field should be the first
     * member of the object when present.  class OID is a special type that is a 12 byte id which
     * is likely to be unique to the system.  You may also use other types for _id's.
     * When _id field is missing from a BSON object, on an insert the database may insert one
     * automatically in certain circumstances.
     *
     * Typical contents of the BSON ObjectID is a 12-byte value consisting of a 4-byte timestamp (seconds since epoch),
     * in the highest order 4 bytes followed by a 5 byte value unique to this machine AND process, followed by a 3 byte
     * counter.
     * 
     * TODO:: explain endianness
     *
     * Warning: You MUST call OID::justForked() after a fork(). This ensures that this process will generate unique OIDs.
     */
    class OID {
    public:

        /**
         * Functor compatible with std::hash for std::unordered_{map,set}
         * Warning: The hash function is subject to change. Do not use in cases where hashes need
         *          to be consistent across versions.
         */
        struct Hasher {
            size_t operator() (const OID& oid) const;
        };

        // Need to decay _data to a char*
        OID() : _data(), _view(static_cast<char*>(_data)) {}

        enum {
            kOIDSize = 12,
            kTimestampSize = 4,
            kUniqueSize = 5,
            kIncrementSize = 3
        };

        /** init from a 24 char hex std::string */
        explicit OID(const std::string &s) : _data(), _view(static_cast<char*>(_data)) { init(s); }

        /** init from a reference to a 12-byte array */
        explicit OID(const unsigned char (&arr)[kOIDSize]) : _data(), _view(static_cast<char*>(_data)) {
            std::memcpy(_data, arr, sizeof(arr));
        }

        /** initialize to 'null' */
        void clear() { std::memset(_data, 0, kOIDSize); }

        const char *getData() const { return _data; }

        bool operator==(const OID& r) const { return compare(r) == 0; }
        bool operator!=(const OID& r) const { return compare(r) != 0; }
        int compare( const OID& other ) const { return memcmp( _data , other._data , kOIDSize ); }
        bool operator<( const OID& other ) const { return compare( other ) < 0; }
        bool operator<=( const OID& other ) const { return compare( other ) <= 0; }

        /** @return the object ID output as 24 hex digits */
        std::string str() const { return toHexLower(_data, kOIDSize); }
        std::string toString() const { return str(); }
        /** @return the random/sequential part of the object ID as 6 hex digits */
        std::string toIncString() const { return toHexLower(getInc()._inc, kIncrementSize); }

        static OID gen() { OID o; o.init(); return o; }

        /** sets the contents to a new oid / randomized value */
        void init();

        /** init from a 24 char hex std::string */
        void init( const std::string& s );

        /** Set to the min/max OID that could be generated at given timestamp. */
        void init( Date_t date, bool max=false );

        time_t asTimeT();
        Date_t asDateT() { return asTimeT() * (long long)1000; }

        // True iff the OID is not empty
        bool isSet() const {
            char zero[kOIDSize] = {0};
            return memcmp(_data, zero, kOIDSize) != 0;
        }

        /**
         * this is not consistent
         * do not store on disk
         */
        void hash_combine(size_t &seed) const;

        /** call this after a fork to update the process id */
        static void justForked();

        static unsigned getMachineId(); // features command uses
        static void regenMachineId();

    private:
        // Timestamp is 4 bytes so we just use int32_t
        typedef int32_t Timestamp;
        // Wrappers so we can return stuff by value.
        class Unique {
        public:
            Unique() : _unique() {}
            static Unique genUnique();
            uint8_t _unique[kUniqueSize]; 
        };

        class Increment {
        public:
            static Increment nextIncrement();
            uint8_t _inc[kIncrementSize];
        };
        
        friend struct endian::ByteOrderConverter<Increment>;

        void setTimestamp(const Timestamp timestamp);
        void setUnique(const Unique unique);
        void setIncrement(const Increment inc);

        Timestamp getTimestamp() const;
        Unique    getUnique() const;
        Increment getInc() const;

        char _data[kOIDSize];
        DataView _view;

        static Unique _machineUnique;
    };

    std::ostream& operator<<( std::ostream &s, const OID &o );
    inline StringBuilder& operator<< (StringBuilder& s, const OID& o) { return (s << o.str()); }

    /** Formatting mode for generating JSON from BSON.
        See <http://dochub.mongodb.org/core/mongodbextendedjson>
        for details.
    */
    enum JsonStringFormat {
        /** strict RFC format */
        Strict,
        /** 10gen format, which is close to JS format.  This form is understandable by
            javascript running inside the Mongo server via eval() */
        TenGen,
        /** Javascript JSON compatible */
        JS
    };

} // namespace mongo
