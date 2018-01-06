# MySQL uuid to binary UDF functions

### General info

MySQL UDF functions implemented in C++ for storing UUID's in the optimal way as described in [MariaDB blog][1] and [Percona blog][2].

As described in the above articles there are few problems with UUID:

- UUID has 36 characters which makes it bulky.
- InnoDB stores data in the PRIMARY KEY order and all the secondary keys also contain PRIMARY KEY. So having UUID as PRIMARY KEY makes the index bigger which can not be fit into the memory
- Inserts are random and the data is scattered.

The articles explains how to store UUID in an efficient way by re-arranging timestamp part of UUID. 

#### API

This module includes two functions to convert a UUID into the ordered binary format and the other way around. 

The functions are:
- `uuid_to_bin` - convert a uuid string into the ordered binary format
- `bin_to_uuid` - convert the ordered binary format into the uuid string 
 
#### Compilation

Just run `make compile` in the project root. This should work on linux and Mac OS X. Compiling scripts are not
tested for other platforms.

#### Installation

After you compile you must install it and tell MySQL about it ([More details][3]). 
First you need to locate the plugin directory. This directory is given by the value of the `plugin_dir` system variable. 
Usually located in `/usr/lib/mysql/plugin/` in linux. Use `SHOW VARIABLES LIKE 'plugin_dir';` to locate this.
 
- Copy the shared object to the server's plugin directory (see above) and name it `uuid2bin.so`
- Inform mysql about the new functions by running: 

```sql
CREATE FUNCTION uuid_to_bin RETURNS STRING SONAME 'uuid2bin.so';
CREATE FUNCTION bin_to_uuid RETURNS STRING SONAME 'uuid2bin.so';
```

#### Testing

```
SELECT BIN_TO_UUID(UUID_TO_BIN('6ccd780c-baba-1026-9564-0040f4311e29')) = '6ccd780c-baba-1026-9564-0040f4311e29';
SELECT BIN_TO_UUID(UUID_TO_BIN(NULL)) IS NULL;
SELECT BIN_TO_UUID(UUID_TO_BIN('invalid')) IS NULL;
```

#### Benchmarks

We run 10 million times each function and compare the results with the same methods implemented as stored functions.

###### Stored functions version:

```sql
DELIMITER //

CREATE FUNCTION UuidToBin(_uuid BINARY(36))
        RETURNS BINARY(16)
        LANGUAGE SQL  DETERMINISTIC  CONTAINS SQL  SQL SECURITY INVOKER
    RETURN
        UNHEX(CONCAT(SUBSTR(_uuid, 15, 4), SUBSTR(_uuid, 10, 4), 
                     SUBSTR(_uuid,  1, 8), SUBSTR(_uuid, 20, 4), 
                     SUBSTR(_uuid, 25) ));

CREATE FUNCTION UuidFromBin(_bin BINARY(16))
        RETURNS BINARY(36)
        LANGUAGE SQL  DETERMINISTIC  CONTAINS SQL  SQL SECURITY INVOKER
    RETURN
        LCASE(CONCAT_WS('-', HEX(SUBSTR(_bin,  5, 4)), HEX(SUBSTR(_bin,  3, 2)),
                             HEX(SUBSTR(_bin,  1, 2)), HEX(SUBSTR(_bin,  9, 2)),
                             HEX(SUBSTR(_bin, 11))));
//
DELIMITER ;
```

###### Results

```
SET @loops=10000000;
SET @uuid='6ccd780c-baba-1026-9564-0040f4311e29';
SET @bin=UUID_TO_BIN(@uuid);

SELECT BENCHMARK(@loops, UUID_TO_BIN(@uuid));
SELECT BENCHMARK(@loops, UuidToBin(@uuid));

SELECT BENCHMARK(@loops, BIN_TO_UUID(@bin));
SELECT BENCHMARK(@loops, UuidFromBin(@bin));

```

|Function            | Time for 1M executions
|:------------------:|:-------------------------:|
| UUID_TO_BIN        | 3.1 sec 			         |
| UuidToBin          | 1 min 9.88 sec            |
| BIN_TO_UUID        | 3.11 sec                  |
| UuidFromBin        | 1 min 30.27 sec           |

###### Conclusion

As expected the UDF versions are much faster than the stored functions. 


[1]:https://mariadb.com/kb/en/library/guiduuid-performance/
[2]:https://www.percona.com/blog/2014/12/19/store-uuid-optimized-way/
[3]:http://dev.mysql.com/doc/refman/5.7/en/udf-compiling.html
