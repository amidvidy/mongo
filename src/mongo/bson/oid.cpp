// @file oid.cpp

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

#include "mongo/platform/basic.h"

#include "mongo/bson/oid.h"

#include <boost/functional/hash.hpp>
#include <boost/scoped_ptr.hpp>

#include "mongo/base/init.h"
#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/platform/atomic_word.h"

#include "mongo/platform/process_id.h"
#include "mongo/platform/random.h"

#define verify MONGO_verify

namespace mongo {

    namespace {
        boost::scoped_ptr<PseudoRandom> entropy;
        boost::scoped_ptr<AtomicUInt64> counter;
        
        const std::size_t kTimestampOffset = 0;
        const std::size_t kUniqueOffset = kTimestampOffset + OID::kTimestampSize;  // 4
        const std::size_t kIncOffset = kUniqueOffset + OID::kUniqueSize;  // 9
   }

    // Set in initializer and regenMachineId()
    OID::Unique OID::_machineUnique = OID::Unique();

    // TODO: figure out prereqs... probably everything
    MONGO_INITIALIZER(OIDEntropy)(InitializerContext* context) {
        // We use a secureRandom to initialize the PRNG. According to Andy we can't use time as 
        // a seed, so we use the OS's pool of secure entropy.
        boost::scoped_ptr<SecureRandom> seedSrc(SecureRandom::create()); 
        entropy.reset(new PseudoRandom(seedSrc->nextInt64()));
        counter.reset(new AtomicUInt64(entropy->nextInt64()));

        OID::regenMachineId();
        return Status::OK();
    }

    // Specializations of ByteOrderConverter so we can use readBE/writeBE
    namespace endian {
        template<>
        struct ByteOrderConverter<OID::Increment> {
            inline static OID::Increment nativeToBig(OID::Increment i) {
// TODO make cleaner with export macros
#if MONGO_BYTE_ORDER == 1234  // little endian
                std::swap(i._inc[0], i._inc[2]);
#endif
                return i;
            }
            
            inline static OID::Increment bigToNative(OID::Increment i) {
#if MONGO_BYTE_ORDER == 1234  // little endian
                std::swap(i._inc[0], i._inc[2]);
#endif
                return i;
            }
        };
    }

    inline OID::Increment OID::Increment::nextIncrement() {
        uint64_t nextCtr = counter->fetchAndAdd(1);
        // We shift the lowest 3 bytes up to the highest position so we can
        // avoid pointer arithmetic
        nextCtr <<= sizeof(uint64_t) - OID::kIncrementSize;
        
        OID::Increment incr;
        // Copy the last 3 bytes
        std::memcpy(incr._inc, &nextCtr, kIncrementSize);
        
        return incr;
    }

    inline OID::Unique OID::Unique::genUnique() {
        int64_t rand = entropy->nextInt64();
        OID::Unique u;
        std::memcpy(u._unique, &rand, OID::kUniqueSize);
        return u;
    }

    void OID::setTimestamp(const OID::Timestamp timestamp) {
        _view.writeBE(timestamp, kTimestampOffset);
    }

    void OID::setUnique(const OID::Unique unique) {
        // Byte order doesn't matter here
        _view.writeNative(unique, kUniqueOffset);
    }

    void OID::setIncrement(const OID::Increment inc) {
        _view.writeBE(inc, kIncOffset);
    }

    OID::Timestamp OID::getTimestamp() const {
        return _view.readBE<Timestamp>(kTimestampOffset);
    }

    OID::Unique OID::getUnique() const {
        // Byte order doesn't matter here
        return _view.readNative<Unique>(kUniqueOffset);
    }

    OID::Increment OID::getInc() const {
        return _view.readBE<Increment>(kUniqueOffset);
    }

    void OID::hash_combine(size_t &seed) const {
        uint32_t v;
        for (int i = 0; i < kOIDSize; i += sizeof(uint32_t)) {
            memcpy(&v, _data + i, sizeof(int32_t));
            boost::hash_combine(seed, v);
        }
    }

    size_t OID::Hasher::operator() (const OID& oid) const {
        size_t seed = 0;
        oid.hash_combine(seed);
        return seed;
    }

    std::ostream& operator<<( std::ostream &s, const OID &o ) {
        s << o.str();
        return s;
    }

    void OID::regenMachineId() {
        _machineUnique = Unique::genUnique();
    }

    unsigned OID::getMachineId() {
        uint32_t ret = 0;
        std::memcpy(&ret, _machineUnique._unique, sizeof(uint32_t));
        return ret;
    }

    void OID::justForked() {
        regenMachineId();
    }

    void OID::init() {
        // each set* method handles endianness
        setTimestamp(time(0));
        setUnique(_machineUnique);
        setIncrement(Increment::nextIncrement());
    }

    void OID::init( const std::string& s ) {
        verify( s.size() == 24 );
        const char *p = s.c_str();
        for (std::size_t i = 0; i < kOIDSize; i++) {
            _data[i] = fromHex(p);
            p += 2;
        }
    }

    void OID::init(Date_t date, bool max) {
        setTimestamp(uint32_t(date / 1000));
        uint64_t rest = max ? std::numeric_limits<uint64_t>::max() : 0u;
        std::memcpy(_view.view(kUniqueOffset), &rest, kUniqueSize + kIncrementSize);
    }

    time_t OID::asTimeT() {
        const Timestamp time = getTimestamp();
        return time;
    }

}
