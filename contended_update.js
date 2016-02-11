/* 
 * Test:
 *   To test concurrent update to a single doc
 * 
 * Setup:
 *   Support standalone, replica
 *   Will not run for shard
 *
 * Note:
 *   Related ticket: SERVER-17195
 */

var run_contended_update = function(nThread, server_addr) {
    var d = db.getSiblingDB("test");
    d.dropDatabase();
    d.foo.insert({_id:1,a:0});

    res = benchRun( { 
        ops : [{ 
            ns : "test.foo",
            op : "update" ,
            query: {_id: 1},
            update : { $inc: {a: 1} } ,
            writeCmd : true }],
        seconds : 300,
        host : server_addr,
        parallel : nThread
    });

    return res;
};

var run_tests = function(server_addr) {
        res = run_contended_update(32, server_addr);
        res = run_contended_update(64, server_addr);
};

run_tests("lazarus:27017");
