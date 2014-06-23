/* Copyright (c) 2012-2013 University of Cape Town
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file
 *
 * Test code for generic parameter support.
 */

#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/extensions/HelperMacros.h>
#include <vector>
#include <string>
#include <sstream>
#include <locale>
#include <memory>
#include <clogs/core.h>
#include "clogs_test.h"
#include "../src/sqlite3.h"
#include "../src/parameters.h"

using namespace clogs::detail;

struct Child
{
    int i2;
    std::string str2;
};

CLOGS_STRUCT_FORWARD(Child)
CLOGS_STRUCT(Child, (i2)(str2))

struct Param
{
    int i;
    Child c;
    std::size_t size;
    std::string str;
    std::vector<unsigned char> blb;
};

CLOGS_STRUCT_FORWARD(Param)
CLOGS_STRUCT(Param, (i)(c)(size)(str)(blb))

class TestParameters : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE(TestParameters);
    CPPUNIT_TEST(testReadFields);
    CPPUNIT_TEST(testReadFieldsEmpty);
    CPPUNIT_TEST(testBindFields);
    CPPUNIT_TEST(testFieldNames);
    CPPUNIT_TEST(testFieldTypes);
    CPPUNIT_TEST(testCompare);
    CPPUNIT_TEST_SUITE_END();

private:
    sqlite3 *db;
    sqlite3_stmt *stmt;

    /// Test that @c readFields is overloaded correctly
    void testReadFields();
    /// Test that @c readFields is overloaded correctly when empty strings are returned
    void testReadFieldsEmpty();
    /// Test that @c bindFields is overloaded correctly
    void testBindFields();
    /// Test that @c fieldNames is overloaded correctly
    void testFieldNames();
    /// Test that @c fieldTypes is overloaded correctly
    void testFieldTypes();
    /// Test that comparison operator is overloaded correctly
    void testCompare();

public:
    virtual void setUp();
    virtual void tearDown();
};
CPPUNIT_TEST_SUITE_REGISTRATION(TestParameters);

void TestParameters::testReadFields()
{
    int status = sqlite3_prepare_v2(
        db, "SELECT i, i2, str2, size, str, blb FROM test", -1, &stmt, NULL);
    CPPUNIT_ASSERT_EQUAL(SQLITE_OK, status);

    status = sqlite3_step(stmt);
    CPPUNIT_ASSERT_EQUAL(SQLITE_ROW, status);

    Param p = Param();
    int pos = readFields(stmt, 0, p);
    CPPUNIT_ASSERT_EQUAL(6, pos);
    CPPUNIT_ASSERT_EQUAL(1, p.i);
    CPPUNIT_ASSERT_EQUAL(2, p.c.i2);
    CPPUNIT_ASSERT_EQUAL(std::string("3"), p.c.str2);
    CPPUNIT_ASSERT_EQUAL(std::size_t(4), p.size);
    CPPUNIT_ASSERT_EQUAL(std::string("5"), p.str);
    CPPUNIT_ASSERT_EQUAL(std::size_t(1), p.blb.size());
    CPPUNIT_ASSERT_EQUAL((unsigned char) 6, p.blb[0]);
}

void TestParameters::testReadFieldsEmpty()
{
    int status = sqlite3_prepare_v2(
        db, "SELECT 0, 0, '', 0, '', X'' FROM test", -1, &stmt, NULL);
    CPPUNIT_ASSERT_EQUAL(SQLITE_OK, status);

    status = sqlite3_step(stmt);
    CPPUNIT_ASSERT_EQUAL(SQLITE_ROW, status);

    Param p = Param();
    int pos = readFields(stmt, 0, p);
    CPPUNIT_ASSERT_EQUAL(6, pos);
    CPPUNIT_ASSERT_EQUAL(0, p.i);
    CPPUNIT_ASSERT_EQUAL(0, p.c.i2);
    CPPUNIT_ASSERT_EQUAL(std::string(""), p.c.str2);
    CPPUNIT_ASSERT_EQUAL(std::size_t(0), p.size);
    CPPUNIT_ASSERT_EQUAL(std::string(""), p.str);
    CPPUNIT_ASSERT_EQUAL(std::size_t(0), p.blb.size());

    status = sqlite3_step(stmt);
    CPPUNIT_ASSERT_EQUAL(SQLITE_DONE, status);
}

void TestParameters::testBindFields()
{
    int status = sqlite3_prepare_v2(
        db, "SELECT ?, ?, ?, ?, ?, ?", -1, &stmt, NULL);
    CPPUNIT_ASSERT_EQUAL(SQLITE_OK, status);

    Param p;
    p.i = 1;
    p.c.i2 = 2;
    p.c.str2 = "3";
    p.size = 4;
    p.str = "5";
    p.blb.push_back(6);
    int pos = bindFields(stmt, 1, p);
    CPPUNIT_ASSERT_EQUAL(7, pos);

    status = sqlite3_step(stmt);
    CPPUNIT_ASSERT_EQUAL(SQLITE_ROW, status);
    Param q;
    readFields(stmt, 0, q);
    CPPUNIT_ASSERT_EQUAL(1, p.i);
    CPPUNIT_ASSERT_EQUAL(2, p.c.i2);
    CPPUNIT_ASSERT_EQUAL(std::string("3"), p.c.str2);
    CPPUNIT_ASSERT_EQUAL(std::size_t(4), p.size);
    CPPUNIT_ASSERT_EQUAL(std::string("5"), p.str);
    CPPUNIT_ASSERT_EQUAL(std::size_t(1), p.blb.size());
    CPPUNIT_ASSERT_EQUAL((unsigned char) 6, p.blb[0]);

    status = sqlite3_step(stmt);
    CPPUNIT_ASSERT_EQUAL(SQLITE_DONE, status);
}

void TestParameters::testFieldNames()
{
    std::vector<const char *> names;
    fieldNames((Param *) NULL, NULL, names);
    CPPUNIT_ASSERT_EQUAL(std::size_t(6), names.size());
    CPPUNIT_ASSERT_EQUAL(std::string("i"), std::string(names[0]));
    CPPUNIT_ASSERT_EQUAL(std::string("i2"), std::string(names[1]));
    CPPUNIT_ASSERT_EQUAL(std::string("str2"), std::string(names[2]));
    CPPUNIT_ASSERT_EQUAL(std::string("size"), std::string(names[3]));
    CPPUNIT_ASSERT_EQUAL(std::string("str"), std::string(names[4]));
    CPPUNIT_ASSERT_EQUAL(std::string("blb"), std::string(names[5]));
}

void TestParameters::testFieldTypes()
{
    std::vector<const char *> types;
    fieldTypes((Param *) NULL, types);
    CPPUNIT_ASSERT_EQUAL(std::size_t(6), types.size());
    CPPUNIT_ASSERT_EQUAL(std::string("INT"), std::string(types[0]));
    CPPUNIT_ASSERT_EQUAL(std::string("INT"), std::string(types[1]));
    CPPUNIT_ASSERT_EQUAL(std::string("TEXT"), std::string(types[2]));
    CPPUNIT_ASSERT_EQUAL(std::string("INT"), std::string(types[3]));
    CPPUNIT_ASSERT_EQUAL(std::string("TEXT"), std::string(types[4]));
    CPPUNIT_ASSERT_EQUAL(std::string("BLOB"), std::string(types[5]));
}

void TestParameters::testCompare()
{
    Param p = Param(), q = Param();

    CPPUNIT_ASSERT(!(p < q));
    CPPUNIT_ASSERT(!(q < p));
    q.blb.push_back(0);
    CPPUNIT_ASSERT(p < q);
    p.c.str2 = "hello";
    CPPUNIT_ASSERT(q < p);
    p.c.i2 = -5;
    CPPUNIT_ASSERT(p < q);
    p.i = 1;
    CPPUNIT_ASSERT(q < p);
}

void TestParameters::setUp()
{
    int status = sqlite3_open_v2(":memory:", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    CPPUNIT_ASSERT_EQUAL(SQLITE_OK, status);

    status = sqlite3_exec(
        db,
        "CREATE TABLE test (i INT, i2 INT, str2 TEXT, size INT, str TEXT, blb BLOB)",
        NULL,
        NULL,
        NULL);
    CPPUNIT_ASSERT_EQUAL(SQLITE_OK, status);
    status = sqlite3_exec(
        db,
        "INSERT INTO test VALUES (1, 2, '3', 4, '5', X'06')",
        NULL,
        NULL,
        NULL);
    CPPUNIT_ASSERT_EQUAL(SQLITE_OK, status);
}

void TestParameters::tearDown()
{
    if (stmt != NULL)
    {
        sqlite3_finalize(stmt);
        stmt = NULL;
    }
    if (db != NULL)
    {
        sqlite3_close(db);
        db = NULL;
    }
}
