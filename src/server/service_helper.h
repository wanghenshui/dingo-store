// Copyright (c) 2023 dingodb.com, Inc. All Rights Reserved
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef DINGODB_SERVER_SERVICE_HELPER_H_
#define DINGODB_SERVER_SERVICE_HELPER_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "butil/compiler_specific.h"
#include "butil/endpoint.h"
#include "common/constant.h"
#include "common/helper.h"
#include "common/logging.h"
#include "fmt/core.h"
#include "meta/store_meta_manager.h"
#include "proto/error.pb.h"
#include "proto/store.pb.h"
#include "server/server.h"

namespace dingodb {

DECLARE_int64(service_helper_store_min_log_elapse);
DECLARE_int64(service_helper_coordinator_min_log_elapse);

class ServiceHelper {
 public:
  template <typename T>
  static void RedirectLeader(std::string addr, T* response);

  template <typename T>
  static pb::node::NodeInfo RedirectLeader(std::string addr);

  static void SetError(pb::error::Error* error, int errcode, const std::string& errmsg);
  static void SetError(pb::error::Error* error, const std::string& errmsg);

  static butil::Status ValidateRegionEpoch(const pb::common::RegionEpoch& req_epoch, store::RegionPtr region);
  static butil::Status GetStoreRegionInfo(store::RegionPtr region, pb::error::Error* error);
  static butil::Status ValidateRegionState(store::RegionPtr region);
  static butil::Status ValidateRange(const pb::common::Range& range);
  static butil::Status ValidateKeyInRange(const pb::common::Range& range, const std::vector<std::string_view>& keys);
  static butil::Status ValidateRangeInRange(const pb::common::Range& region_range, const pb::common::Range& req_range);
  static butil::Status ValidateRegion(store::RegionPtr region, const std::vector<std::string_view>& keys);
  static butil::Status ValidateIndexRegion(store::RegionPtr region, const std::vector<int64_t>& vector_ids);
  static butil::Status ValidateClusterReadOnly();
};

template <typename T>
pb::node::NodeInfo ServiceHelper::RedirectLeader(std::string addr) {
  auto raft_endpoint = Helper::StrToEndPoint(addr);
  if (raft_endpoint.port == 0) {
    DINGO_LOG(WARNING) << fmt::format("[redirect][addr({})] invalid addr.", addr);
    return {};
  }

  // From local store map query.
  auto node_info =
      Server::GetInstance().GetStoreMetaManager()->GetStoreServerMeta()->GetNodeInfoByRaftEndPoint(raft_endpoint);
  if (node_info.id() == 0) {
    // From remote node query.
    Helper::GetNodeInfoByRaftLocation(Helper::EndPointToLocation(raft_endpoint), node_info);
  }

  if (!node_info.server_location().host().empty()) {
    // transform ip to hostname
    Server::GetInstance().Ip2Hostname(*node_info.mutable_server_location()->mutable_host());
  }

  DINGO_LOG(INFO) << fmt::format("[redirect][addr({})] redirect leader, node_info: {}", addr,
                                 node_info.ShortDebugString());

  return node_info;
}

template <typename T>
void ServiceHelper::RedirectLeader(std::string addr, T* response) {
  auto node_info = RedirectLeader<T>(addr);
  if (node_info.id() != 0) {
    Helper::SetPbMessageErrorLeader(node_info, response);
  } else {
    response->mutable_error()->set_store_id(Server::GetInstance().Id());
  }
}

// Handle service request in execute queue.
class ServiceTask : public TaskRunnable {
 public:
  using Handler = std::function<void(void)>;
  ServiceTask(Handler handle) : handle_(handle) {}
  ~ServiceTask() override = default;

  std::string Type() override { return "SERVICE_TASK"; }

  void Run() override { handle_(); }

 private:
  Handler handle_;
};

// Wrapper brpc service closure for log.
template <typename T, typename U>
class ServiceClosure : public google::protobuf::Closure {
 public:
  ServiceClosure(const std::string& method_name, google::protobuf::Closure* done, const T* request, U* response)
      : method_name_(method_name), done_(done), request_(request), response_(response) {
    start_time_ = Helper::TimestampNs();
    DINGO_LOG(DEBUG) << fmt::format("[service.{}] Receive request: {}", method_name_,
                                    request_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength));
  }
  ~ServiceClosure() override = default;

  void Run() override;

 private:
  std::string method_name_;
  uint64_t start_time_;

  google::protobuf::Closure* done_;
  const T* request_;
  U* response_;
};

template <typename T, typename U>
void ServiceClosure<T, U>::Run() {
  std::unique_ptr<ServiceClosure<T, U>> self_guard(this);
  brpc::ClosureGuard done_guard(done_);

  uint64_t elapsed_time = Helper::TimestampNs() - start_time_;

  if (response_->error().errcode() != 0) {
    // Set leader redirect info(pb.Error.leader_location).
    if (response_->error().errcode() == pb::error::ERAFT_NOTLEADER) {
      ServiceHelper::RedirectLeader(response_->error().errmsg(), response_);
      response_->mutable_error()->set_errmsg(fmt::format("Not leader({}) on region {}, please redirect leader({}).",
                                                         Server::GetInstance().ServerAddr(),
                                                         request_->context().region_id(), response_->error().errmsg()));
    }

    DINGO_LOG(ERROR) << fmt::format(
        "[service.{}][request_id({})][elapsed(ns)({})] Request failed, response: {} request: {}", method_name_,
        request_->request_info().request_id(), elapsed_time,
        response_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength),
        request_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength));
  } else {
    if (BAIDU_UNLIKELY(elapsed_time >= FLAGS_service_helper_store_min_log_elapse)) {
      DINGO_LOG(INFO) << fmt::format(
          "[service.{}][request_id({})][elapsed(ns)({})] Request finish, response: {} request: {}", method_name_,
          request_->request_info().request_id(), elapsed_time,
          response_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength),
          request_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength));
    } else {
      DINGO_LOG(DEBUG) << fmt::format(
          "[service.{}][request_id({})][elapsed(ns)({})] Request finish, response: {} request: {}", method_name_,
          request_->request_info().request_id(), elapsed_time,
          response_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength),
          request_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength));
    }
  }

  Helper::SetPbMessageResponseInfo(response_, elapsed_time);
}

template <>
inline void ServiceClosure<pb::index::VectorCalcDistanceRequest, pb::index::VectorCalcDistanceResponse>::Run() {
  std::unique_ptr<ServiceClosure<pb::index::VectorCalcDistanceRequest, pb::index::VectorCalcDistanceResponse>>
      self_guard(this);
  brpc::ClosureGuard done_guard(done_);

  uint64_t elapsed_time = Helper::TimestampNs() - start_time_;

  if (response_->error().errcode() != 0) {
    DINGO_LOG(ERROR) << fmt::format(
        "[service.{}][request_id({})][elapsed(ns)({})] Request failed, response: {} request: {}", method_name_,
        request_->request_info().request_id(), elapsed_time,
        response_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength),
        request_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength));
  } else {
    if (BAIDU_UNLIKELY(elapsed_time >= FLAGS_service_helper_store_min_log_elapse)) {
      DINGO_LOG(INFO) << fmt::format(
          "[service.{}][request_id({})][elapsed(ns)({})] Request finish, response: {} request: {}", method_name_,
          request_->request_info().request_id(), elapsed_time,
          response_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength),
          request_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength));
    } else {
      DINGO_LOG(DEBUG) << fmt::format(
          "[service.{}][request_id({})][elapsed(ns)({})] Request finish, response: {} request: {}", method_name_,
          request_->request_info().request_id(), elapsed_time,
          response_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength),
          request_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength));
    }
  }

  Helper::SetPbMessageResponseInfo(response_, elapsed_time);
}

// Wrapper brpc service closure for log.
template <typename T, typename U>
class CoordinatorServiceClosure : public google::protobuf::Closure {
 public:
  CoordinatorServiceClosure(const std::string& method_name, google::protobuf::Closure* done, const T* request,
                            U* response)
      : method_name_(method_name), done_(done), request_(request), response_(response) {
    start_time_ = Helper::TimestampNs();
    DINGO_LOG(DEBUG) << fmt::format("[service.{}] Receive request: {}", method_name_,
                                    request_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength));
  }
  ~CoordinatorServiceClosure() override = default;

  void Run() override;

 private:
  std::string method_name_;
  uint64_t start_time_;

  google::protobuf::Closure* done_;
  const T* request_;
  U* response_;
};

template <typename T, typename U>
void CoordinatorServiceClosure<T, U>::Run() {
  std::unique_ptr<CoordinatorServiceClosure<T, U>> self_guard(this);
  brpc::ClosureGuard done_guard(done_);

  uint64_t elapsed_time = Helper::TimestampNs() - start_time_;

  if (response_->error().errcode() != 0) {
    // Set leader redirect info(pb.Error.leader_location).
    if (response_->error().errcode() == pb::error::ERAFT_NOTLEADER) {
      response_->mutable_error()->set_errmsg(fmt::format("Not leader({}), please redirect leader({}).",
                                                         Server::GetInstance().ServerAddr(),
                                                         response_->error().errmsg()));
    }

    DINGO_LOG(ERROR) << fmt::format(
        "[service.{}][request_id({})][elapsed(ns)({})] Request failed, response: {} request: {}", method_name_,
        request_->request_info().request_id(), elapsed_time,
        response_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength),
        request_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength));
  } else {
    if (BAIDU_UNLIKELY(elapsed_time >= FLAGS_service_helper_coordinator_min_log_elapse)) {
      DINGO_LOG(INFO) << fmt::format(
          "[service.{}][request_id({})][elapsed(ns)({})] Request finish, response: {} request: {}", method_name_,
          request_->request_info().request_id(), elapsed_time,
          response_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength),
          request_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength));
    } else {
      DINGO_LOG(DEBUG) << fmt::format(
          "[service.{}][request_id({})][elapsed(ns)({})] Request finish, response: {} request: {}", method_name_,
          request_->request_info().request_id(), elapsed_time,
          response_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength),
          request_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength));
    }
  }

  Helper::SetPbMessageResponseInfo(response_, elapsed_time);
}

template <typename T, typename U>
class NoContextServiceClosure : public google::protobuf::Closure {
 public:
  NoContextServiceClosure(const std::string& method_name, google::protobuf::Closure* done, const T* request,
                          U* response)
      : method_name_(method_name), done_(done), request_(request), response_(response) {
    start_time_ = Helper::TimestampNs();
    DINGO_LOG(DEBUG) << fmt::format("[service.{}] Receive request: {}", method_name_,
                                    request_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength));
  }
  ~NoContextServiceClosure() override = default;

  void Run() override;

 private:
  std::string method_name_;
  uint64_t start_time_;

  google::protobuf::Closure* done_;
  const T* request_;
  U* response_;
};

template <typename T, typename U>
void NoContextServiceClosure<T, U>::Run() {
  std::unique_ptr<NoContextServiceClosure<T, U>> self_guard(this);
  brpc::ClosureGuard done_guard(done_);

  uint64_t elapsed_time = Helper::TimestampNs() - start_time_;

  if (response_->error().errcode() != 0) {
    // Set leader redirect info(pb.Error.leader_location).
    if (response_->error().errcode() == pb::error::ERAFT_NOTLEADER) {
      ServiceHelper::RedirectLeader(response_->error().errmsg(), response_);
      response_->mutable_error()->set_errmsg(fmt::format("Not leader({}), please redirect leader({}).",
                                                         Server::GetInstance().ServerAddr(),
                                                         response_->error().errmsg()));
    }

    DINGO_LOG(ERROR) << fmt::format(
        "[service.{}][request_id({})][elapsed(ns)({})] Request failed, response: {} request: {}", method_name_,
        request_->request_info().request_id(), elapsed_time,
        response_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength),
        request_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength));
  } else {
    DINGO_LOG(DEBUG) << fmt::format(
        "[service.{}][request_id({})][elapsed(ns)({})] Request finish, response: {} request: {}", method_name_,
        request_->request_info().request_id(), elapsed_time,
        response_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength),
        request_->ShortDebugString().substr(0, Constant::kLogPrintMaxLength));
  }

  Helper::SetPbMessageResponseInfo(response_, elapsed_time);
}

}  // namespace dingodb

#endif  // DINGODB_SERVER_SERVICE_HELPER_H_