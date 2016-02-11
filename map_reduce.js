/* 
 * Test:
 *   To test map reduce over 1M doc
 * 
 * Setup:
 *   All target (standalone, replica, sharded)
 *
 * Note:
 *   - Convert MR job execution time to doc_processed_per_second as throughput
 *   - This test stage 1M doc evenly distributed over 200 UID, the map reduce
 *     job will calculate sum of amount based on uid, thus the output will be
 *     total 200 doc, and input is 1M
 */

var server = "lazarus:27017";
var db_name = "test";
var d = db.getSiblingDB(db_name);

// drop database, and re-create index
d.dropDatabase();
d.mr.ensureIndex({uid: 1});

// set random seed to a fixed number
Random.srand(341215145);

var staging_data = function() {
    var mr_db = db.getSiblingDB(db_name);

    for(i =0; i < 200; i++) {
        var bulk = d.mr.initializeUnorderedBulkOp();
        for(j = 0; j < 1000; j++){
            for(k = 0; k < 5; k ++) {
                bulk.insert({
                    uid: i, 
                    amount: Random.randInt(1000000), 
                    status: k});
            }
        }
        bulk.execute( {w: 1});
    }
};

print("Server is : " + server);

// staging data
var s = Date.now();
print("Start staging data: " + s);
staging_data();
var e = Date.now();
print("Done staging data: " + e + ". Total time taken: " + (e - s) + "ms");

// run test here
fmap    = function() { emit( this.uid, this.amount ); };
freduce = function(k, v) { return Array.sum(v) ;};
query   = { out: { reduce: "totals", db: "mr_results", nonAtomic: true} };

s = Date.now();
re = d.mr.mapReduce( fmap, freduce, query);
e = Date.now();
print("Done MR job! Total time taken: " + (e - s) + "ms");
print("MR report time taken is " + re.timeMillis + " ms");

// report results
var pass = true;
var errMsg = "";

// set errMsg and failed flag in case of error
if (re.ok != 1) { pass = false; errMsg = "MapReduce Job return ok != 1"; }

var throughput = re.counts.input * 1000 / re.timeMillis;

// print return value, would like to keep this in Evergreen log
printjson(re);


