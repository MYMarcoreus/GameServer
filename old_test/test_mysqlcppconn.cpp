#include<mysql_connection.h>
#include<mysql_driver.h>
#include<mysql_error.h>
#include<cppconn/prepared_statement.h>
#include<cppconn/statement.h>


using namespace std;

int main() {
    sql::mysql::MySQL_Driver * driver = sql::mysql::get_driver_instance();
    assert(driver != nullptr);

    //! 连接mysql
    sql::Connection * conn = driver->connect("127.0.0.1:3306", "root", "0");
    assert(conn != nullptr);

    //! use xx数据库;
    conn->setSchema("mysql"); // use mysql;
    cout << "数据库名：" << conn->getSchema() << endl;

    //! 创建语句对象
    sql::Statement * state = conn->createStatement();
    assert(state != nullptr);

    //! 执行SQL语句(此时行指针指向第一行之前的结点)
    sql::ResultSet * rst = state->executeQuery("select * from user");
    assert(rst != nullptr);

    //! 获取查询结果的元数据：就是表头等信息，不包含结果数据
    sql::ResultSetMetaData * metaData = rst->getMetaData();
    assert(metaData != nullptr);

    size_t nCol = metaData->getColumnCount();
    size_t nRow = rst->rowsCount();
    cout << "列数：" << nCol << endl;
    cout << "行数：" << nRow << endl;


    //! 遍历输出表头
    for(size_t i = 1; i <= nCol ; i++){
        printf("%7s", metaData->getColumnName(i).c_str());
    } cout << endl;

    for(size_t i = 1; i <= nCol ; i++){
        cout << metaData->getColumnLabel(i) << "-"
             << metaData->getColumnType(i) << "-"
             << metaData->getSchemaName(i) << "-"
             << metaData->getTableName(i) << "-"
             << metaData->getColumnTypeName(i) << endl;
    } cout << endl;


    // rst->beforeFirst(); // 将行指针移动回第一行之前
    // rst->afterLast();   // 将行指针移动到最后一行之后

    //! 遍历所有行
    while(rst->next()) {
        // 遍历行的所有元素
        for(size_t i = 1; i <= nCol ; i++){
            printf("%7s", rst->getString(i).c_str());
        } cout << endl;
    } cout << endl;

    printf("---DONE---\n");

    return 0;
}