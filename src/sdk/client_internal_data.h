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

#ifndef DINGODB_SDK_CLIENT_IMPL_H_
#define DINGODB_SDK_CLIENT_IMPL_H_

#include <cstdint>
#include <memory>

#include "coordinator/coordinator_interaction.h"
#include "sdk/client.h"
#include "sdk/client_stub.h"
#include "sdk/meta_cache.h"
#include "sdk/status.h"

namespace dingodb {
namespace sdk {

class Client::Data {
 public:
  Data(const Data&) = delete;
  const Data& operator=(const Data&) = delete;

  Data() : init(false), stub(nullptr) {}

  ~Data() = default;

  bool init;
  std::unique_ptr<ClientStub> stub;
};

}  // namespace sdk
}  // namespace dingodb

#endif  // DINGODB_SDK_CLIENT_IMPL_H_