/*    Copyright 2014 MongoDB Inc.
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

#include "mongo/bson/oid.h"
#include "mongo/platform/endian.h"
#include "mongo/unittest/unittest.h"

namespace {

    using mongo::OID;

    TEST(Increasing, Simple) {
        OID o1 = OID::gen();
        OID o2 = OID::gen();
        ASSERT_TRUE(o1 < o2);
    }

    TEST(IsSet, Simple) {
        OID o;
        ASSERT_FALSE(o.isSet());
        o.init();
        ASSERT_TRUE(o.isSet());
    }

    TEST(JustForked, Simple) {
        OID o1 = OID::gen();
        OID::justForked();
        OID o2 = OID::gen();

        ASSERT_TRUE(std::memcmp(o1.getUnique()._unique, o2.getUnique()._unique,
                                OID::kUniqueSize) != 0);
    }

    TEST(TimestampIsBigEndian, Endianness) {
        OID o1; //zeroed
        OID::Timestamp ts = 123;
        o1.setTimestamp(ts);

        int32_t ts_big = mongo::endian::nativeToBig<int32_t>(123);

        const char* oidBytes = o1.getData();
        ASSERT(std::memcmp(&ts_big, oidBytes, sizeof(int32_t)) == 0);
    }

    TEST(IncrementIsBigEndian, Endianness) {
        OID o1; // zeroed
        OID::Increment incr;
        //Increment is a 3 byte counter
#if MONGO_BYTE_ORDER == 1234 // little endian
        incr._inc[0] = 0xDEu;
        incr._inc[1] = 0xADu;
        incr._inc[2] = 0xBEu;
#else // big endian
        incr._inc[0] = 0xBEu;
        incr._inc[1] = 0xADu;
        incr._inc[2] = 0xDEu;
#endif

        o1.setIncrement(incr);

        const char* oidBytes = o1.getData();
        oidBytes += OID::kTimestampSize + OID::kUniqueSize;

        // now at start of increment
        ASSERT_EQUALS(uint8_t(oidBytes[0]), 0xBEu);
        ASSERT_EQUALS(uint8_t(oidBytes[1]), 0xADu);
        ASSERT_EQUALS(uint8_t(oidBytes[2]), 0xDEu);
    }

    TEST(Basic, Deserialize) {
        OID o1;

        char* mutBytes = o1.getDataMutable();

        uint8_t OIDbytes[] = {
            0xDEu, 0xADu, 0xBEu, 0xEFu,        // timestamp is -559038737 (signed)
            0x00u, 0x00u, 0x00u, 0x00u, 0x00u, // unique is 0
            0x11u, 0x22u, 0x33u                // increment is 1122867
        };

        std::memcpy(mutBytes, &OIDbytes, OID::kOIDSize);

        ASSERT_EQUALS(o1.getTimestamp(), -559038737);
        OID::Unique u = o1.getUnique();
        for (std::size_t i = 0; i < OID::kUniqueSize; ++i) {
            ASSERT_EQUALS(u._unique[i], 0x00u);
        }
        OID::Increment i = o1.getIncrement();

        // construct a uint32_t from increment
        // recall that i is a 3 byte integer, now in native endianness
        uint32_t incr =
#if MONGO_BYTE_ORDER == 1234
            // little endian
            ((uint32_t(i._inc[2]) << 16)) |
            ((uint32_t(i._inc[1]) << 8))  |
            uint32_t(i._inc[0]);
#elif MONGO_BYTE_ORDER == 4321
            // big endian
            ((uint32_t(i._inc[0]) << 16)) |
            ((uint32_t(i._inc[1]) << 8))  |
            uint32_t(i._inc[2]);
#endif
        ASSERT_EQUALS(1122867u, incr);
    }
}
