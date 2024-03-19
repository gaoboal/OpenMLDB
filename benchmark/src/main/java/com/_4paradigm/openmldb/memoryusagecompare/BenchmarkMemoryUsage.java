package com._4paradigm.openmldb.memoryusagecompare;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.io.InputStream;
import java.sql.SQLException;
import java.util.*;


public class BenchmarkMemoryUsage {
    private static final Logger logger = LoggerFactory.getLogger(BenchmarkMemoryUsage.class);
    private static int keyLength;
    private static int valueLength;
    private static int valuePerKey;
    private static String[] totalKeyNums;
    private static final RedisExecutor redis = new RedisExecutor();
    private static final OpenMLDBExecutor opdb = new OpenMLDBExecutor();
    private static final InputStream configStream = BenchmarkMemoryUsage.class.getClassLoader().getResourceAsStream("memory.properties");
    private static final Properties config = new Properties();
    private static final Summary summary = new Summary();
    private static final int batchKeys = 100;

    public static void main(String[] args) {
        logger.info("Start benchmark test: Compare memory usage with Redis.");
        try {
            parseConfig();

            BenchmarkMemoryUsage m = new BenchmarkMemoryUsage();
            for (String keyNum : totalKeyNums) {
                int kn = Integer.parseInt(keyNum);
                m.clearData();
                long time1 = System.currentTimeMillis();
                m.insertData(kn);
                long time2 = System.currentTimeMillis();
                m.getMemUsage(kn);
                summary.printSummary();
                long time3 = System.currentTimeMillis();
                logger.info("Insert elapsed time: " + (time2-time1) + ", Total elapse time: " + (time3-time1));
            }
            m.closeConn();
            summary.printSummary();
            logger.info("Done.");
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private static void parseConfig() throws IOException {
        logger.info("start parse test configs ... ");
        config.load(configStream);
        keyLength = Integer.parseInt(config.getProperty("KEY_LENGTH"));
        valueLength = Integer.parseInt(config.getProperty("VALUE_LENGTH"));
        valuePerKey = Integer.parseInt(config.getProperty("VALUE_PER_KEY"));
        totalKeyNums = config.getProperty("TOTAL_KEY_NUM").split(",");

        logger.info("test config: \n" +
                "\tKEY_LENGTH: " + keyLength + "\n" +
                "\tVALUE_LENGTH: " + valueLength + "\n" +
                "\tVALUE_PER_KEY: " + valuePerKey + "\n" +
                "\tTOTAL_KEY_NUM: " + Arrays.toString(totalKeyNums) + "\n"
        );
    }

    public BenchmarkMemoryUsage() throws IOException, SQLException {
        redis.initializeJedis(config, configStream);
        opdb.initializeOpenMLDB(config, configStream);
        opdb.initOpenMLDBEnv();
    }

    private void clearData() throws SQLException, InterruptedException {
        logger.info("delete all data in redis and openmldb, and wait for the asynchronous operation to complete ... ");
        redis.clear();
        opdb.clear();
        Thread.sleep(10 * 1000);
        logger.info("Done. All test data deleted.");
    }

    private void closeConn() {
        redis.close();
        opdb.close();
    }

    private void insertData(int keyNum) {
        logger.info("start test: key size: " + keyNum + ", values per key: " + valuePerKey);
        int count = 0;
        HashMap<String, ArrayList<String>> keyValues = new HashMap<>();
        for (int keyIdx = 0; keyIdx <= keyNum; keyIdx++) {
            if (count >= batchKeys) {
                redis.insert(keyValues);
                opdb.insert(keyValues);
                count = 0;
                keyValues.clear();
            }
            String key = generateRandomString(keyLength);
            ArrayList<String> values = new ArrayList<>();
            for (int valIdx = 0; valIdx < valuePerKey; valIdx++) {
                values.add(generateRandomString(valueLength));
            }
            keyValues.put(key, values);
            count ++;
        }
    }

    private String generateRandomString(int length) {
        String characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        StringBuilder result = new StringBuilder();
        Random random = new Random();
        while (length-- > 0) {
            result.append(characters.charAt(random.nextInt(characters.length())));
        }
        return result.toString();
    }

    private void getMemUsage(int kn) throws InterruptedException {
        Thread.sleep(10 * 1000);
        HashMap<String, Long> knRes = new HashMap<>();
        HashMap<String, String> redisInfoMap = redis.getRedisInfo();
        HashMap<String, String> openMLDBMem = opdb.getTableStatus();

        knRes.put("redis", Long.parseLong(redisInfoMap.get("used_memory")));
        knRes.put("openmldb", Long.parseLong(openMLDBMem.get(OpenMLDBTableStatusField.MEMORY_DATA_SiZE.name())));
        summary.summary.put(kn + "", knRes);
    }

}
