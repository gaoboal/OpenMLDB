/*
 * dbms_sdk.cc
 * Copyright (C) 4paradigm.com 2019 wangtaize <wangtaize@4paradigm.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sdk/dbms_sdk.h"
#include <iostream>
#include <memory>
#include <utility>
#include "analyser/analyser.h"
#include "brpc/channel.h"
#include "node/node_manager.h"
#include "parser/parser.h"
#include "plan/planner.h"
#include "proto/dbms.pb.h"
#include "sdk/result_set_impl.h"
#include "sdk/tablet_sdk.h"

namespace fesql {
namespace sdk {

class DBMSSdkImpl : public DBMSSdk {
 public:
    explicit DBMSSdkImpl(const std::string &endpoint);
    ~DBMSSdkImpl();
    bool Init();

    void CreateDatabase(const std::string &catalog, sdk::Status *status);

    std::shared_ptr<TableSet> GetTables(const std::string &catalog,
                                        sdk::Status *status);

    std::vector<std::string> GetDatabases(sdk::Status *status);

    std::shared_ptr<ResultSet> ExecuteQuery(const std::string &catalog,
                                            const std::string &sql,
                                            sdk::Status *status);

 private:
    bool InitTabletSdk();

 private:
    ::brpc::Channel *channel_;
    std::string endpoint_;
    std::shared_ptr<TabletSdk> tablet_sdk_;
};

DBMSSdkImpl::DBMSSdkImpl(const std::string &endpoint)
    : channel_(NULL), endpoint_(endpoint), tablet_sdk_() {}

DBMSSdkImpl::~DBMSSdkImpl() { delete channel_; }

bool DBMSSdkImpl::Init() {
    channel_ = new ::brpc::Channel();
    brpc::ChannelOptions options;
    int ret = channel_->Init(endpoint_.c_str(), &options);
    if (ret != 0) {
        return false;
    }
    return InitTabletSdk();
}

bool DBMSSdkImpl::InitTabletSdk() {
    ::fesql::dbms::DBMSServer_Stub stub(channel_);
    ::fesql::dbms::GetTabletRequest request;
    ::fesql::dbms::GetTabletResponse response;
    brpc::Controller cntl;
    stub.GetTablet(&cntl, &request, &response, NULL);
    if (cntl.Failed()) {
        return false;
    }
    if (response.endpoints_size() <= 0) {
        return false;
    }
    tablet_sdk_ = CreateTabletSdk(response.endpoints().Get(0));
    if (tablet_sdk_) return true;
    return false;
}

std::shared_ptr<TableSet> DBMSSdkImpl::GetTables(const std::string &catalog,
                                                 sdk::Status *status) {
    if (status == NULL) {
        return std::shared_ptr<TableSetImpl>();
    }
    ::fesql::dbms::DBMSServer_Stub stub(channel_);
    ::fesql::dbms::GetTablesRequest request;
    ::fesql::dbms::GetTablesResponse response;
    brpc::Controller cntl;
    request.set_db_name(catalog);
    stub.GetTables(&cntl, &request, &response, NULL);
    if (cntl.Failed()) {
        status->code = common::kRpcError;
        status->msg = "fail to call remote";
        return std::shared_ptr<TableSetImpl>();
    } else {
        std::shared_ptr<TableSetImpl> table_set(
            new TableSetImpl(response.tables()));
        status->code = response.status().code();
        status->msg = response.status().msg();
        return table_set;
    }
}

std::vector<std::string> DBMSSdkImpl::GetDatabases(sdk::Status *status) {
    if (status == NULL) {
        return std::vector<std::string>();
    }
    ::fesql::dbms::DBMSServer_Stub stub(channel_);
    ::fesql::dbms::GetDatabasesRequest request;
    ::fesql::dbms::GetDatabasesResponse response;
    std::vector<std::string> names;
    brpc::Controller cntl;
    stub.GetDatabases(&cntl, &request, &response, NULL);
    if (cntl.Failed()) {
        status->code = common::kRpcError;
        status->msg = "fail to call remote";
    } else {
        for (auto name : response.names()) {
            names.push_back(name);
        }
        status->code = response.status().code();
        status->msg = response.status().msg();
    }
    return names;
}

std::shared_ptr<ResultSet> DBMSSdkImpl::ExecuteQuery(const std::string &catalog,
                                                     const std::string &sql,
                                                     sdk::Status *status) {
    std::shared_ptr<ResultSetImpl> empty;
    node::NodeManager node_manager;
    parser::FeSQLParser parser;
    analyser::FeSQLAnalyser analyser(&node_manager);
    plan::SimplePlanner planner(&node_manager);
    DLOG(INFO) << "start to execute script from dbms:\n" << sql;

    base::Status sql_status;
    node::NodePointVector parser_trees;
    parser.parse(sql, parser_trees, &node_manager, sql_status);
    if (0 != sql_status.code) {
        LOG(WARNING) << sql_status.msg;
        status->code = sql_status.code;
        status->msg = sql_status.msg;
        return empty;
    }

    /*node::NodePointVector query_trees;
    analyser.Analyse(parser_trees, query_trees, sql_status);
    if (0 != sql_status.code) {
        status->code = sql_status.code;
        status->msg = sql_status.msg;
        LOG(WARNING) << status->msg;
        return empty;
    }*/

    node::PlanNodeList plan_trees;
    planner.CreatePlanTree(parser_trees, plan_trees, sql_status);

    if (0 != sql_status.code) {
        status->code = sql_status.code;
        status->msg = sql_status.msg;
        LOG(WARNING) << status->msg;
        return empty;
    }

    node::PlanNode *plan = plan_trees[0];

    if (nullptr == plan) {
        status->msg = "fail to execute plan : plan null";
        status->code = common::kPlanError;
        LOG(WARNING) << status->msg;
        return empty;
    }

    switch (plan->GetType()) {
        case node::kPlanTypeSelect: {
            return tablet_sdk_->Query(catalog, sql, status);
        }
        case node::kPlanTypeInsert: {
            tablet_sdk_->Insert(catalog, sql, status);
            return empty;
        }
        case node::kPlanTypeCreate: {
            node::CreatePlanNode *create =
                dynamic_cast<node::CreatePlanNode *>(plan);
            ::fesql::dbms::DBMSServer_Stub stub(channel_);
            ::fesql::dbms::AddTableRequest add_table_request;
            ::fesql::dbms::AddTableResponse response;
            add_table_request.set_db_name(catalog);
            ::fesql::type::TableDef *table = add_table_request.mutable_table();
            table->set_catalog(catalog);
            plan::TransformTableDef(create->GetTableName(),
                                    create->GetColumnDescList(), table,
                                    sql_status);
            if (0 != sql_status.code) {
                status->code = sql_status.code;
                status->msg = sql_status.msg;
                LOG(WARNING) << status->msg;
                return empty;
            }
            brpc::Controller cntl;
            stub.AddTable(&cntl, &add_table_request, &response, NULL);
            if (cntl.Failed()) {
                status->code = -1;
                status->msg = "fail to call remote";
            } else {
                status->code = response.status().code();
                status->msg = response.status().msg();
            }
            return empty;
        }

        default: {
            status->msg = "fail to execute script with unSuppurt type" +
                          node::NameOfPlanNodeType(plan->GetType());
            status->code = fesql::common::kUnSupport;
            return empty;
        }
    }
}
void DBMSSdkImpl::CreateDatabase(const std::string &catalog,
                                 sdk::Status *status) {
    if (status == NULL) return;
    ::fesql::dbms::DBMSServer_Stub stub(channel_);
    ::fesql::dbms::AddDatabaseRequest request;
    request.set_name(catalog);
    ::fesql::dbms::AddDatabaseResponse response;
    brpc::Controller cntl;
    stub.AddDatabase(&cntl, &request, &response, NULL);
    if (cntl.Failed()) {
        status->code = -1;
        status->msg = "fail to call remote";
    } else {
        status->code = response.status().code();
        status->msg = response.status().msg();
    }
}

std::shared_ptr<DBMSSdk> CreateDBMSSdk(const std::string &endpoint) {
    DBMSSdkImpl *sdk_impl = new DBMSSdkImpl(endpoint);
    if (sdk_impl->Init()) {
        return std::shared_ptr<DBMSSdkImpl>(sdk_impl);
    }
    return std::shared_ptr<DBMSSdk>();
}

}  // namespace sdk
}  // namespace fesql
