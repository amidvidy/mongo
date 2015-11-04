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

#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kASIO

#include "mongo/platform/basic.h"

#include <chrono>
#include <exception>
#include <algorithm>
#include <limits>

#include "mongo/base/status_with.h"
#include "mongo/bson/bsonmisc.h"
#include "mongo/client/connection_string.h"
#include "mongo/executor/async_stream_factory.h"
#include "mongo/executor/async_stream_interface.h"
#include "mongo/executor/async_timer_asio.h"
#include "mongo/executor/network_interface_asio.h"
#include "mongo/executor/network_interface_asio_test_utils.h"
#include "mongo/executor/network_interface_impl.h"
#include "mongo/executor/task_executor.h"
#include "mongo/stdx/memory.h"
#include "mongo/unittest/integration_test.h"
#include "mongo/unittest/unittest.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/log.h"
#include "mongo/util/net/hostandport.h"
#include "mongo/util/scopeguard.h"
#include "mongo/util/timer.h"

namespace mongo {
namespace executor {
namespace {

const std::size_t numOperations = 1000000;

struct LatencyStats {
    LatencyStats(std::vector<stdx::chrono::microseconds> opTimes) {
        std::vector<std::tuple<stdx::chrono::microseconds, std::size_t>> opTimesWithIndexes;

        for (std::size_t i = 0; i < opTimes.size(); ++i) {
            opTimesWithIndexes.emplace_back(opTimes[i], i);
        }

        std::sort(std::begin(opTimesWithIndexes), std::end(opTimesWithIndexes));

        min = opTimesWithIndexes[0];
        max = opTimesWithIndexes[opTimesWithIndexes.size() - 1];
        percentile50 = opTimesWithIndexes[std::floor(opTimesWithIndexes.size() * 0.5)];
        percentile99 = opTimesWithIndexes[std::floor(opTimesWithIndexes.size() * 0.99)];
        percentile999 = opTimesWithIndexes[std::floor(opTimesWithIndexes.size() * 0.999)];
        percentile9999 = opTimesWithIndexes[std::floor(opTimesWithIndexes.size() * 0.9999)];
    }

    using time_with_index = std::tuple<stdx::chrono::microseconds, std::size_t>;

    time_with_index min;
    time_with_index max;

    time_with_index percentile50;
    time_with_index percentile99;
    time_with_index percentile999;
    time_with_index percentile9999;
};

using clock = stdx::chrono::high_resolution_clock;

std::vector<stdx::chrono::microseconds> executeOps(std::size_t operations, NetworkInterface* net) {
    auto fixture = unittest::getFixtureConnectionString();
    auto server = fixture.getServers()[0];

    std::atomic<int> remainingOps(operations);  // NOLINT
    stdx::mutex mtx;
    stdx::condition_variable cv;

    // This lambda function is declared here since it is mutually recursive with another callback
    // function
    stdx::function<void()> func;

    // only accessed by this thread
    std::vector<clock::time_point> startTimes;
    startTimes.reserve(operations);

    // only accessed by asio thread (until cv wait ends)
    std::vector<clock::time_point> endTimes;
    endTimes.reserve(operations);

    // only accessed by this thread
    std::vector<stdx::chrono::microseconds> opTimes;
    opTimes.reserve(operations);

    const auto bsonObjPing = BSON("ping" << 1);

    const auto callback = [&](StatusWith<RemoteCommandResponse> resp) {
        uassertStatusOK(resp);
        endTimes.emplace_back(clock::now());

        if (--remainingOps) {
            return func();
        }

        stdx::unique_lock<stdx::mutex> lk(mtx);
        cv.notify_one();
    };

    func = [&]() {
        startTimes.emplace_back(clock::now());
        net->startCommand(makeCallbackHandle(),
                          {server, "admin", bsonObjPing, bsonObjPing, Milliseconds(-1)},
                          callback);
    };

    func();

    stdx::unique_lock<stdx::mutex> lk(mtx);
    cv.wait(lk, [&] { return remainingOps.load() == 0; });


    for (std::size_t i = 0; i < startTimes.size(); ++i) {
        opTimes.emplace_back(
            stdx::chrono::duration_cast<stdx::chrono::microseconds>(endTimes[i] - startTimes[i]));
    }
    // Throw away the first op since it includes connect latency.
    opTimes.erase(std::begin(opTimes));
    return opTimes;
}

TEST(NetworkInterfaceASIO, SerialPerf) {
    NetworkInterfaceASIO::Options options{};
    options.streamFactory = stdx::make_unique<AsyncStreamFactory>();
    options.timerFactory = stdx::make_unique<AsyncTimerFactoryASIO>();
    NetworkInterfaceASIO net{std::move(options)};

    net.startup();
    auto guard = MakeGuard([&] { net.shutdown(); });

    auto opTimes = executeOps(numOperations, &net);

    LatencyStats stats{std::move(opTimes)};

    auto to_string = [](LatencyStats::time_with_index twi) -> std::string {
        std::stringstream ss;
        ss << "duration - " << std::get<0>(twi) << ", opIndex - " << std::get<1>(twi);
        return ss.str();
    };

    log() << "latency stats:";
    log() << "\tmin - " << to_string(stats.min);
    log() << "\t50th percentile - " << to_string(stats.percentile50);
    log() << "\t99th percentile - " << to_string(stats.percentile99);
    log() << "\t99.9th percentile - " << to_string(stats.percentile999);
    log() << "\t99.99th percentile - " << to_string(stats.percentile9999);
    log() << "\tmax - " << to_string(stats.max);
}

}  // namespace
}  // namespace executor
}  // namespace mongo
