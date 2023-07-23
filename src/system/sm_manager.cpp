/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sm_manager.h"

#include <sys/stat.h>
#include <unistd.h>

#include <fstream>

#include "index/ix.h"
#include "record/rm.h"
#include "record_printer.h"

/**
 * @description: 判断是否为一个文件夹
 * @return {bool} 返回是否为一个文件夹
 * @param {string&} db_name 数据库文件名称，与文件夹同名
 */
bool SmManager::is_dir(const std::string& db_name) {
    struct stat st;
    return stat(db_name.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

/**
 * @description: 创建数据库，所有的数据库相关文件都放在数据库同名文件夹下
 * @param {string&} db_name 数据库名称
 */
void SmManager::create_db(const std::string& db_name) {
    if (is_dir(db_name)) {
        throw DatabaseExistsError(db_name);
    }
    //为数据库创建一个子目录
    std::string cmd = "mkdir " + db_name;
    if (system(cmd.c_str()) < 0) {  // 创建一个名为db_name的目录
        throw UnixError();
    }
    if (chdir(db_name.c_str()) < 0) {  // 进入名为db_name的目录
        throw UnixError();
    }
    //创建系统目录
    DbMeta *new_db = new DbMeta();
    new_db->name_ = db_name;

    // 注意，此处ofstream会在当前目录创建(如果没有此文件先创建)和打开一个名为DB_META_NAME的文件
    std::ofstream ofs(DB_META_NAME);
    // 将new_db中的信息，按照定义好的operator<<操作符，写入到ofs打开的DB_META_NAME文件中
    ofs << *new_db;  // 注意：此处重载了操作符<<

    delete new_db;

    // 创建日志文件
    disk_manager_->create_file(LOG_FILE_NAME);

    // 回到根目录
    if (chdir("..") < 0) {
        throw UnixError();
    }
}

/**
 * @description: 删除数据库，同时需要清空相关文件以及数据库同名文件夹
 * @param {string&} db_name 数据库名称，与文件夹同名
 */
void SmManager::drop_db(const std::string& db_name) {
    if (!is_dir(db_name)) {
        throw DatabaseNotFoundError(db_name);
    }
    std::string cmd = "rm -r " + db_name;
    if (system(cmd.c_str()) < 0) {
        throw UnixError();
    }
}

/**
 * @description: 打开数据库，找到数据库对应的文件夹，并加载数据库元数据和相关文件
 * @param {string&} db_name 数据库名称，与文件夹同名
 */
void SmManager::open_db(const std::string& db_name) {
    //lsy
    //1.判断是否是目录并进入
    if (!is_dir(db_name)) {
        throw DatabaseNotFoundError(db_name);
    }
    if (chdir(db_name.c_str()) < 0) {
        throw UnixError();
    }

    //2.加载数据库元数据和相关文件
    //lsy:模仿了create_db的代码
    // 打开一个名为DB_META_NAME的文件
    std::ifstream ofs(DB_META_NAME);

    // 读入ofs打开的DB_META_NAME文件，写入到db_
    ofs >> db_;

    //将数据库中包含的表 导入到当前fhs
    for(const auto& table : db_.tabs_) {
        fhs_.insert({table.first,rm_manager_->open_file(table.first)});
        const auto& indices = table.second.indexes;
        for(const auto&index:table.second.indexes) {
            auto index_name = ix_manager_->get_index_name(table.first,index.cols);
            ihs_.emplace(index_name,
                         ix_manager_->open_index(index_name));
        }
    }

}

/**
 * @description: 把数据库相关的元数据刷入磁盘中
 */
void SmManager::flush_meta() {
    // 默认清空文件
    std::ofstream ofs(DB_META_NAME);
    ofs << db_;
}

/**
 * @description: 关闭数据库并把数据落盘
 */
void SmManager::close_db() {
    //lsy
    //1.将数据落盘
    flush_meta();

    //2.关闭当前fhs_的文件及退出目录
    for(auto &pair : fhs_)
        rm_manager_->close_file(pair.second.get());

    // 回到根目录
    if (chdir("..") < 0) {
        throw UnixError();
    }
}

/**
 * @description: 显示所有的表,通过测试需要将其结果写入到output.txt,详情看题目文档
 * @param {Context*} context 
 */
void SmManager::show_tables(Context* context) {
    std::fstream outfile;
    outfile.open("output.txt", std::ios::out | std::ios::app);
    outfile << "| Tables |\n";
    RecordPrinter printer(1);
    printer.print_separator(context);
    printer.print_record({"Tables"}, context);
    printer.print_separator(context);
    for (auto &entry : db_.tabs_) {
        auto &tab = entry.second;
        printer.print_record({tab.name}, context);
        outfile << "| " << tab.name << " |\n";
    }
    printer.print_separator(context);
    outfile.close();
}

void SmManager::show_index(const std::string& tab_name,Context* context) {
    auto index_metas = db_.get_table(tab_name).indexes;
    std::fstream outfile;
    outfile.open("output.txt", std::ios::out | std::ios::app);
    RecordPrinter printer(3);
    for(auto &index:index_metas){
        outfile << "| " << tab_name << " | unique | " << "(" ;
        std::stringstream ss;
        ss << "(";
        auto it = index.cols.begin();
        outfile << (*it).name;
        ss << (*it).name;
        it++;
        for( ;it != index.cols.end();++it){
            outfile << ","<< (*it).name;
            ss << "," << (*it).name;
        }

        outfile <<") |\n";
        ss << ")";
        printer.print_record({tab_name, "unique", ss.str()}, context);
    }
}


/**
 * @description: 显示表的元数据
 * @param {string&} tab_name 表名称
 * @param {Context*} context 
 */
void SmManager::desc_table(const std::string& tab_name, Context* context) {
    TabMeta &tab = db_.get_table(tab_name);

    std::vector<std::string> captions = {"Field", "Type", "Index"};
    RecordPrinter printer(captions.size());
    // Print header
    printer.print_separator(context);
    printer.print_record(captions, context);
    printer.print_separator(context);
    // Print fields
    for (auto &col : tab.cols) {
        std::vector<std::string> field_info = {col.name, coltype2str(col.type), col.index ? "YES" : "NO"};
        printer.print_record(field_info, context);
    }
    // Print footer
    printer.print_separator(context);
}

/**
 * @description: 创建表
 * @param {string&} tab_name 表的名称
 * @param {vector<ColDef>&} col_defs 表的字段
 * @param {Context*} context 
 */
void SmManager::create_table(const std::string& tab_name, const std::vector<ColDef>& col_defs, Context* context) {
    if (db_.is_table(tab_name)) {
        throw TableExistsError(tab_name);
    }
    // Create table meta
    int curr_offset = 0;
    TabMeta tab;
    tab.name = tab_name;
    for (auto &col_def : col_defs) {
        ColMeta col = {.tab_name = tab_name,
                       .name = col_def.name,
                       .type = col_def.type,
                       .len = col_def.len,
                       .offset = curr_offset,
                       .index = false};
        curr_offset += col_def.len;
        tab.cols.push_back(col);
    }
    // Create & open record file
    int record_size = curr_offset;  // record_size就是col meta所占的大小（表的元数据也是以记录的形式进行存储的）
    rm_manager_->create_file(tab_name, record_size);
    db_.tabs_[tab_name] = tab;
    // fhs_[tab_name] = rm_manager_->open_file(tab_name);
    fhs_.emplace(tab_name, rm_manager_->open_file(tab_name));
    flush_meta();
}

/**
 * @description: 删除表
 * @param {string&} tab_name 表的名称
 * @param {Context*} context
 */
void SmManager::drop_table(const std::string& tab_name, Context* context) {
    // lsy
    // 如果不存在table才需要报错
    if (!db_.is_table(tab_name)) {
        throw TableNotFoundError(tab_name);
    }
    auto& indexes = db_.tabs_[tab_name].indexes;
    for(auto &index:indexes) {
        drop_index(tab_name, index.cols,context);
    }
    auto tab = fhs_.find(tab_name)->second.get();
    //关闭这个table文件
    rm_manager_->close_file(tab);
    buffer_pool_manager_->delete_all_pages(tab->GetFd());
    //删除db中对这个表的记录
    //tab与fhs
    fhs_.erase(tab_name);
    db_.tabs_.erase(tab_name);
    flush_meta();
    //删除表文件
    disk_manager_->destroy_file(tab_name);
}

/**
 * @description: 创建索引
 * @param {string&} tab_name 表的名称
 * @param {vector<string>&} col_names 索引包含的字段名称
 * @param {Context*} context
 */
void SmManager::create_index(const std::string& tab_name, const std::vector<std::string>& col_names, Context* context) {
    if(!db_.is_table(tab_name)) {
        throw TableNotFoundError(tab_name);
    }
    auto &table = db_.get_table(tab_name); // 获取table
    if(table.is_index(col_names)) {
        // 如果已经存在index，需要抛出异常
        throw IndexExistsError(tab_name, col_names);
    }
    IndexMeta indexMeta{.tab_name=tab_name};
    auto col_num = static_cast<int>(col_names.size());
    int tot_len = 0;
    std::vector<ColMeta> index_cols;
    for(const auto&col_name:col_names) {
        // 从col中找到对应名字的列
        ColMeta  col = table.get_col_meta(col_name);
        index_cols.emplace_back(col);
        tot_len+=col.len;
    }
    ix_manager_->create_index(tab_name,index_cols);
    auto index_name = ix_manager_->get_index_name(tab_name,index_cols);
    assert(ihs_.count(index_name)==0); // 确保之前没有创建过该index
    ihs_.emplace(index_name,ix_manager_->open_index(tab_name, index_cols));
    auto index_handler = ihs_.find(index_name)->second.get();
    indexMeta.col_num = col_num;
    indexMeta.col_tot_len = tot_len;
    indexMeta.cols=index_cols;
    table.indexes.emplace_back(indexMeta);

    auto table_file_handle = fhs_.find(tab_name)->second.get();
    RmScan rm_scan(table_file_handle);
    Transaction transaction(0); // TODO (AntiO2) 事务
    while (!rm_scan.is_end()) {
        auto rid = rm_scan.rid();
        auto origin_key = table_file_handle->get_record(rid,context);
        auto key = origin_key->key_from_rec(index_cols);
        index_handler->insert_entry(key->data,rid, &transaction);
        rm_scan.next();
    }
    flush_meta();
}

/**
 * @description: 删除索引
 * @param {string&} tab_name 表名称
 * @param {vector<string>&} col_names 索引包含的字段名称
 * @param {Context*} context
 */
void SmManager::drop_index(const std::string& tab_name, const std::vector<std::string>& col_names, Context* context) {
    auto ix_name = ix_manager_->get_index_name(tab_name, col_names);
    if(!ix_manager_->exists(ix_name)) {
        throw IndexNotFoundError(tab_name, col_names);
    }
    auto ihs_iter  = ihs_.find(ix_name);
    if(ihs_iter==ihs_.end()) {
        throw IndexNotFoundError(tab_name, col_names);
    }

    buffer_pool_manager_->delete_all_pages(ihs_iter->second->getFd());
    ix_manager_->close_index(ihs_iter->second.get());
    ix_manager_->destroy_index(ix_name); // 删除索引文件
    ihs_.erase(ihs_iter); // check(AntiO2) 此处删除迭代器是否有错

    db_.tabs_[tab_name].remove_index(col_names);

    flush_meta();
}

/**
 * @description: 删除索引
 * @param {string&} tab_name 表名称
 * @param {vector<ColMeta>&} 索引包含的字段元数据
 * @param {Context*} context
 */
void SmManager::drop_index(const std::string& tab_name, const std::vector<ColMeta>& cols, Context* context) {
    std::vector<std::string> col_name;
    for(const auto& col :cols) {
        col_name.emplace_back(col.name);
    }
    drop_index(tab_name,col_name,context);
}