#define MONGO_LOG_DEFAULT_COMPONENT ::mongo::logger::LogComponent::kStorage

#include <stdexcept>
#include <thread>
#include <wiredtiger.h>

#include "mongo/platform/basic.h"
#include "mongo/unittest/temp_dir.h"
#include "mongo/unittest/unittest.h"
#include "mongo/util/assert_util.h"
#include "mongo/util/debugger.h"
#include "mongo/util/log.h"
#include "mongo/util/mongoutils/str.h"
#include "mongo/util/scopeguard.h"

namespace mongo {

void check(int wiredtiger_ret) {
    if (wiredtiger_ret) {
        severe() << wiredtiger_strerror(wiredtiger_ret);
        breakpoint();
        ::exit(1);
    }
}

TEST(ReverseCursorRepro, Repro) {
    unittest::TempDir tempDir("ReverseCursorRepo");

    WT_CONNECTION* conn = nullptr;
    const char* table = "table:reverse_cursor_repro";

    check(wiredtiger_open(tempDir.path().c_str(), nullptr, "create,statistics=(all),", &conn));
    ON_BLOCK_EXIT([&] { check(conn->close(conn, nullptr)); });

    {
        WT_SESSION* s = nullptr;
        check(conn->open_session(conn, nullptr, "isolation=snapshot", &s));

        // same as WiredTigerRecordStore::generateCreateString.
        const char* config =
            "type=file,"
            "memory_page_max=10m,"
            "split_pct=90,"
            "leaf_value_max=64MB,"
            "checksum=on,"
            "block_compressor=snappy,"
            "key_format=q,"
            "value_format=u";

        check(s->begin_transaction(s, nullptr));
        ON_BLOCK_EXIT([&] { check(s->commit_transaction(s, nullptr)); });
        check(s->create(s, table, config));
    }

    std::atomic<bool> done{false};
    std::atomic<uint64_t> nextId{1};
    std::thread inserter;

    inserter = std::thread([conn, &done, &nextId, table] {
        WT_SESSION* insert_sess = nullptr;

        std::array<char, sizeof(std::uint64_t)> buf;

        check(conn->open_session(conn, nullptr, "isolation=snapshot", &insert_sess));
        ON_BLOCK_EXIT([&] { insert_sess->close(insert_sess, nullptr); });
        while (!done.load()) {
            check(insert_sess->begin_transaction(insert_sess, nullptr));
            ON_BLOCK_EXIT([&] { check(insert_sess->commit_transaction(insert_sess, nullptr)); });

            WT_CURSOR* cursor = nullptr;
            WT_ITEM item;

            std::int64_t id = nextId.fetch_add(1);

            std::memcpy(buf.data(), &id, sizeof(id));

            item.data = buf.data();
            item.size = sizeof(id);

            check(insert_sess->open_cursor(insert_sess, table, nullptr, nullptr, &cursor));

            ON_BLOCK_EXIT([&] { check(cursor->close(cursor)); });

            cursor->set_key(cursor, id);
            cursor->set_value(cursor, &item);

            check(cursor->insert(cursor));
        }


    });

    ON_BLOCK_EXIT([&] {
        done.store(true);
        inserter.join();
    });

    uint64_t highest_seen_recno = 0;

    {
        WT_SESSION* read_sess = nullptr;
        check(conn->open_session(conn, nullptr, "isolation=snapshot", &read_sess));
        ON_BLOCK_EXIT([&] { check(read_sess->close(read_sess, nullptr)); });
        while (true) {
            check(read_sess->begin_transaction(read_sess, nullptr));

            ON_BLOCK_EXIT([&] { check(read_sess->commit_transaction(read_sess, nullptr)); });

            WT_CURSOR* cursor = nullptr;
            std::uint64_t cur_recno = 0;

            check(read_sess->open_cursor(read_sess, table, nullptr, nullptr, &cursor));

            ON_BLOCK_EXIT([&] { check(cursor->close(cursor)); });

            // Reset cursor
            check(cursor->reset(cursor));

            // Cursor should be positioned at the last record.
            int ret = cursor->prev(cursor);
            if (ret == WT_NOTFOUND) {
                // It is possible to have no records at first while the inserter starts.
                continue;
            }

            check(ret);

            check(cursor->get_key(cursor, &cur_recno));

            // We should always see a record number at least as high as the highest we have
            // seen.
            // That is, cur_recno should increase monotonically.
            if (!(cur_recno >= highest_seen_recno)) {
                log() << "NOT INCREASING: cur_recno=" << cur_recno
                      << ", highest_seen_recno=" << highest_seen_recno;
                break;
            }

            highest_seen_recno = cur_recno;
        }
    }
}

}  // namespace mongo
